// RDS decoding on top of TEF668X::readRds(). Group layouts: IEC 62106 (RDS) / NRSC-4 (RBDS).
#include "TEF668XRds.h"

#include <Arduino.h>
#include <string.h>

#include "TEF668X.h"

namespace {
constexpr uint8_t kBufferedGroups = 22;  // user manual 4.2.2: RDS buffer depth
constexpr uint8_t kRtEnd = 0x0D;         // RadioText end mark

char printable(uint8_t c) { return (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : ' '; }
}  // namespace

TEF668XRds::TEF668XRds(TEF668X& radio) : radio_(radio) {}

void TEF668XRds::setPolicy(tef::RdsPolicy policy) {
  policy_ = policy;
  switch (policy) {
    case tef::RdsPolicy::Strict: cfg_ = {0, 3, 3}; break;
    case tef::RdsPolicy::Normal: cfg_ = {1, 2, 2}; break;
    case tef::RdsPolicy::Fast: cfg_ = {2, 1, 1}; break;
    case tef::RdsPolicy::Dx: cfg_ = {2, 2, 2}; break;
    case tef::RdsPolicy::Custom: break;  // keep the custom values
  }
}

void TEF668XRds::setCustomPolicy(const tef::RdsPolicyConfig& config) {
  cfg_ = config;
  if (cfg_.maxError > 2) cfg_.maxError = 2;  // level 3 is never usable
  if (cfg_.psConfirm == 0) cfg_.psConfirm = 1;
  if (cfg_.rtConfirm == 0) cfg_.rtConfirm = 1;
  policy_ = tef::RdsPolicy::Custom;
}

void TEF668XRds::clear() {
  synced_ = lost_ = piValid_ = tp_ = ta_ = ms_ = psNew_ = rtNew_ = rtAb_ = false;
  lastErr_ = pty_ = 0;
  pi_ = 0;
  groups_ = accepted_ = 0;
  memset(ps_, ' ', 8);
  ps_[8] = '\0';
  memset(psCand_, ' ', sizeof(psCand_));
  memset(psCount_, 0, sizeof(psCount_));
  memset(rt_, 0, sizeof(rt_));
  memset(rtCount_, 0, sizeof(rtCount_));
  rtEnd_ = -1;
  afCount_ = 0;
  clock_ = tef::RdsClock();
}

uint8_t TEF668XRds::update() {
  if (!radio_.isReady() || radio_.band() != tef::Band::FM) return 0;
  if (radio_.frequency() != lastFreq_) {
    lastFreq_ = radio_.frequency();
    clear();
  }

  // With a Davn pin wired, skip the I2C traffic when the chip has nothing (active low).
  const tef::BoardConfig& b = radio_.board();
  for (uint8_t i = 0; i < 3; ++i) {
    const bool davn = b.gpio[i].feature == tef::GpioFeature::Davn || b.gpio[i].feature == tef::GpioFeature::QsiDavn;
    if (davn && b.gpio[i].mcuPin >= 0 && digitalRead(b.gpio[i].mcuPin) == HIGH) return 0;
  }

  uint8_t n = 0;
  for (; n < kBufferedGroups; ++n) {
    tef::RdsGroup g;
    if (radio_.readRds(g, true) != tef::Error::Ok) break;
    synced_ = g.synced();
    if (g.dataLost()) lost_ = true;
    if (!g.available()) break;
    decode(g);
  }
  return n;
}

uint8_t TEF668XRds::quality() const {
  return groups_ == 0 ? 0 : static_cast<uint8_t>((static_cast<uint32_t>(accepted_) * 100U) / groups_);
}

void TEF668XRds::decode(const tef::RdsGroup& g) {
  lastErr_ = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    if (g.error(i) > lastErr_) lastErr_ = g.error(i);
  }
  if (groups_ < 0xFFFF) ++groups_;

  // PI: block A (and C' in version B groups).
  if (accept(g, 0)) {
    pi_ = g.block[0];
    piValid_ = true;
  }
  if (!accept(g, 1)) return;  // block B carries the group type: nothing is usable without it
  if (accepted_ < 0xFFFF) ++accepted_;

  const uint16_t b = g.block[1];
  const uint8_t type = b >> 12;
  const bool versionB = (b >> 11) & 1;
  tp_ = (b >> 10) & 1;
  pty_ = (b >> 5) & 0x1F;
  const uint16_t c = g.block[2];
  const uint16_t d = g.block[3];
  const bool cOk = accept(g, 2);
  const bool dOk = accept(g, 3);

  switch (type) {
    case 0: {  // basic tuning: TA, M/S, PS, AF (0A)
      ta_ = (b >> 4) & 1;
      ms_ = (b >> 3) & 1;
      if (dOk) psPair(b & 0x03, printable(d >> 8), printable(d & 0xFF));
      if (!versionB && cOk) {
        addAf(c >> 8);
        addAf(c & 0xFF);
      }
      break;
    }
    case 2: {  // RadioText
      const bool ab = (b >> 4) & 1;
      if (ab != rtAb_) {  // A/B flag toggles = new text
        rtAb_ = ab;
        memset(rt_, 0, sizeof(rt_));
        memset(rtCount_, 0, sizeof(rtCount_));
        rtEnd_ = -1;
      }
      const uint8_t seg = b & 0x0F;
      if (!versionB) {
        if (cOk && dOk) {
          const char t[4] = {static_cast<char>(c >> 8), static_cast<char>(c & 0xFF), static_cast<char>(d >> 8),
                             static_cast<char>(d & 0xFF)};
          rtChars(seg, t, 4);
        }
      } else if (dOk) {
        const char t[2] = {static_cast<char>(d >> 8), static_cast<char>(d & 0xFF)};
        rtChars(seg, t, 2);
      }
      break;
    }
    case 4: {  // clock time (4A)
      if (!versionB && cOk && dOk) {
        clock_.mjd = (static_cast<uint32_t>(b & 0x03) << 15) | (c >> 1);
        clock_.hour = static_cast<uint8_t>(((c & 0x01) << 4) | (d >> 12));
        clock_.minute = (d >> 6) & 0x3F;
        const int8_t off = d & 0x1F;
        clock_.offsetHalfHours = ((d >> 5) & 1) ? -off : off;
        clock_.valid = clock_.hour < 24 && clock_.minute < 60;
      }
      break;
    }
    default: break;
  }
}

void TEF668XRds::psPair(uint8_t pair, char a, char b) {
  const uint8_t pos = pair * 2;
  if (psCand_[pos] == a && psCand_[pos + 1] == b) {
    if (psCount_[pair] < 255) ++psCount_[pair];
  } else {
    psCand_[pos] = a;
    psCand_[pos + 1] = b;
    psCount_[pair] = 1;
  }
  if (psCount_[pair] >= cfg_.psConfirm && (ps_[pos] != a || ps_[pos + 1] != b)) {
    ps_[pos] = a;
    ps_[pos + 1] = b;
    psNew_ = true;
  }
}

void TEF668XRds::rtChars(uint8_t seg, const char* t, uint8_t n) {
  const uint8_t base = seg * n;
  if (base + n > 64) return;
  // Same content as already stored? (end mark is stored as '\0')
  bool same = rtCount_[seg] > 0;
  for (uint8_t i = 0; i < n && same; ++i) {
    const char want = (t[i] == kRtEnd) ? '\0' : printable(static_cast<uint8_t>(t[i]));
    if (rt_[base + i] != want) same = false;
    if (t[i] == kRtEnd) break;
  }
  if (same) {
    if (rtCount_[seg] < 255) ++rtCount_[seg];
  } else {
    rtCount_[seg] = 1;
  }
  for (uint8_t i = 0; i < n; ++i) {
    if (t[i] == kRtEnd) {
      rtEnd_ = static_cast<int8_t>(seg);
      rt_[base + i] = '\0';
      break;
    }
    rt_[base + i] = printable(static_cast<uint8_t>(t[i]));
  }
  if (rtCount_[seg] == cfg_.rtConfirm) rtNew_ = true;
}

bool TEF668XRds::rtComplete() const {
  const int8_t last = rtEnd_ >= 0 ? rtEnd_ : 15;
  for (int8_t s = 0; s <= last; ++s) {
    if (rtCount_[s] < cfg_.rtConfirm) return false;
  }
  return true;
}

bool TEF668XRds::psChanged() {
  const bool v = psNew_;
  psNew_ = false;
  return v;
}

bool TEF668XRds::rtChanged() {
  const bool v = rtNew_;
  rtNew_ = false;
  return v;
}

void TEF668XRds::addAf(uint8_t code) {
  if (code < 1 || code > 204) return;  // 1...204 = 87.6 ... 107.9 MHz; others are counts / fillers
  for (uint8_t i = 0; i < afCount_; ++i) {
    if (afCodes_[i] == code) return;
  }
  if (afCount_ < sizeof(afCodes_)) afCodes_[afCount_++] = code;
}

uint16_t TEF668XRds::af(uint8_t i) const {
  return i < afCount_ ? static_cast<uint16_t>(8750 + afCodes_[i] * 10) : 0;
}
