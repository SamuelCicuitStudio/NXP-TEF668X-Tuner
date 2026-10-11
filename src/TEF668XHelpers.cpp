// L2 feature helpers and L3 preset engine. Everything here is built on
// TEF668X::write(), so the same range / variant / state checks apply.
#include "TEF668X.h"

#include "TEF668XPlatform.h"

using tef::Cmd;
using tef::detail::CmdDesc;
using tef::detail::readDesc;

namespace {

constexpr uint8_t kNone = 0xFF;  // "this part does not exist" in the WSH map

// Commands of one weak-signal function: Time, Mod, Level, Noise, Mph, Max, Min.
struct WshCmds {
  uint8_t c[7];
};

#define C_(x) static_cast<uint8_t>(Cmd::x)
const WshCmds kWshMap[6] TEF668X_PROGMEM = {
    // Softmute FM / AM
    {{C_(FmSoftmuteTime), kNone, C_(FmSoftmuteLevel), C_(FmSoftmuteNoise), C_(FmSoftmuteMph), C_(FmSoftmuteMax), kNone}},
    {{C_(AmSoftmuteTime), C_(AmSoftmuteMod), C_(AmSoftmuteLevel), kNone, kNone, C_(AmSoftmuteMax), kNone}},
    // Highcut FM / AM
    {{C_(FmHighcutTime), C_(FmHighcutMod), C_(FmHighcutLevel), C_(FmHighcutNoise), C_(FmHighcutMph), C_(FmHighcutMax),
      C_(FmHighcutMin)}},
    {{C_(AmHighcutTime), C_(AmHighcutMod), C_(AmHighcutLevel), kNone, kNone, C_(AmHighcutMax), C_(AmHighcutMin)}},
    // Stereo FM, StHiBlend FM
    {{C_(FmStereoTime), C_(FmStereoMod), C_(FmStereoLevel), C_(FmStereoNoise), C_(FmStereoMph), C_(FmStereoMax),
      C_(FmStereoMin)}},
    {{C_(FmStHiBlendTime), C_(FmStHiBlendMod), C_(FmStHiBlendLevel), C_(FmStHiBlendNoise), C_(FmStHiBlendMph),
      C_(FmStHiBlendMax), C_(FmStHiBlendMin)}},
};
#undef C_

// Row of kWshMap for a function/band, or -1 when that combination does not exist.
int8_t wshRow(tef::WshFunction fn, tef::Band band) {
  const bool am = band == tef::Band::AM;
  switch (fn) {
    case tef::WshFunction::Softmute: return am ? 1 : 0;
    case tef::WshFunction::Highcut: return am ? 3 : 2;
    case tef::WshFunction::Stereo: return am ? -1 : 4;
    case tef::WshFunction::StHiBlend: return am ? -1 : 5;
  }
  return -1;
}

uint8_t wshCmd(int8_t row, uint8_t part) { return TEF668X_READ_U8(&kWshMap[row].c[part]); }

// Integer scale: value * percent / 100.
uint16_t pct(uint16_t value, uint8_t percent) { return static_cast<uint16_t>((static_cast<uint32_t>(value) * percent) / 100U); }

}  // namespace

namespace tef {

uint8_t Quality::signalPercent() const {
  // 10 dBuV -> 0 %, 50 dBuV -> 100 % (level is 0.1 dBuV), then FM noise / multipath
  // penalties (same thresholds as the client's tested sketch, integer maths).
  int32_t s = (static_cast<int32_t>(level) - 100) * 100 / 400;
  if (s < 0) s = 0;
  if (s > 100) s = 100;
  uint16_t v = static_cast<uint16_t>(s);
  if (band == Band::FM) {
    const uint16_t det[2] = {noise, word4};
    for (uint8_t i = 0; i < 2; ++i) {
      if (det[i] > 400) v = pct(v, 90);
      if (det[i] > 550) v = pct(v, 80);
      if (det[i] > 700) v = pct(v, 60);
    }
  }
  return static_cast<uint8_t>(v);
}

BandPlan bandPlan(Region r) {
  switch (r) {
    case Region::USA: return {8750, 10790, 20, 530, 1700, 10, Deemphasis::Us75};
    case Region::Japan: return {7600, 9500, 10, 531, 1602, 9, Deemphasis::Us50};
    case Region::Oirt: return {6590, 7400, 3, 531, 1602, 9, Deemphasis::Us50};
    case Region::Europe:
    default: return {8750, 10800, 10, 531, 1602, 9, Deemphasis::Us50};
  }
}

}  // namespace tef

// ============================================================================
// Weak-signal handling
// ============================================================================

TEF668X::Error TEF668X::setWsh(WshFunction fn, Band band, const WshConfig& c, uint8_t parts) {
  const int8_t row = wshRow(fn, band);
  if (row < 0) return Error::InvalidArg;  // Stereo / StHiBlend exist on FM only

  for (uint8_t part = 0; part < 7; ++part) {
    if (!(parts & (1U << part))) continue;
    const uint8_t id = wshCmd(row, part);
    if (id == kNone) continue;
    int16_t p[4];
    uint8_t n = 0;
    switch (part) {
      case 0: p[0] = c.time.slowAttack; p[1] = c.time.slowDecay; p[2] = c.time.fastAttack; p[3] = c.time.fastDecay; n = 4; break;
      case 1: p[0] = c.mod.enable; p[1] = c.mod.start; p[2] = c.mod.slope; p[3] = c.mod.shift; n = 4; break;
      case 2: p[0] = static_cast<int16_t>(c.level.mode); p[1] = c.level.start; p[2] = c.level.slope; n = 3; break;
      case 3: p[0] = static_cast<int16_t>(c.noise.mode); p[1] = c.noise.start; p[2] = c.noise.slope; n = 3; break;
      case 4: p[0] = static_cast<int16_t>(c.mph.mode); p[1] = c.mph.start; p[2] = c.mph.slope; n = 3; break;
      case 5: p[0] = c.max.mode; p[1] = c.max.limit; n = 2; break;
      case 6: p[0] = c.min.mode; p[1] = c.min.limit; n = 2; break;
    }
    const Cmd cmd = static_cast<Cmd>(id);
    if (part == 5 && cmd == Cmd::FmStereoMax) n = 1;  // Stereo_Max has only a mode
    const Error e = write(cmd, p, n);
    if (e != Error::Ok) return e;
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::wshDefaults(WshFunction fn, Band band, WshConfig& c) const {
  const int8_t row = wshRow(fn, band);
  if (row < 0) return Error::InvalidArg;
  c = WshConfig();
  for (uint8_t part = 0; part < 7; ++part) {
    const uint8_t id = wshCmd(row, part);
    if (id == kNone) continue;
    int16_t p[7] = {0};
    uint8_t n = 0;
    if (chipDefaults(static_cast<Cmd>(id), p, n) != Error::Ok) continue;
    switch (part) {
      case 0: c.time = {static_cast<uint16_t>(p[0]), static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2]), static_cast<uint16_t>(p[3])}; break;
      case 1: c.mod = {p[0] != 0, static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2]), static_cast<uint16_t>(p[3])}; break;
      case 2: c.level = {static_cast<WshMode>(p[0]), static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2])}; break;
      case 3: c.noise = {static_cast<WshMode>(p[0]), static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2])}; break;
      case 4: c.mph = {static_cast<WshMode>(p[0]), static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2])}; break;
      case 5: c.max = {static_cast<uint8_t>(p[0]), static_cast<uint16_t>(n > 1 ? p[1] : 0)}; break;
      case 6: c.min = {static_cast<uint8_t>(p[0]), static_cast<uint16_t>(p[1])}; break;
    }
  }
  return Error::Ok;
}

// ============================================================================
// Defaults and regions
// ============================================================================

TEF668X::Error TEF668X::chipDefaults(Cmd cmd, int16_t* out, uint8_t& count) const {
  if (static_cast<uint8_t>(cmd) >= static_cast<uint8_t>(Cmd::Count) || out == nullptr) return Error::InvalidArg;
  const CmdDesc d = readDesc(cmd);
  if (!(d.flags & tef::detail::kFlagHasDefaults)) return Error::InvalidArg;
  count = d.nMax;
  for (uint8_t i = 0; i < d.nMax; ++i) {
    out[i] = static_cast<int16_t>(TEF668X_READ_U16(&tef::detail::kDefaults[d.param + i]));
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::restoreChipDefaults(Band band) {
  if (!bandEnabled(band)) return Error::BandDisabled;
  for (uint8_t id = 0; id < static_cast<uint8_t>(Cmd::Count); ++id) {
    const Cmd cmd = static_cast<Cmd>(id);
    const CmdDesc d = readDesc(cmd);
    if (d.module != static_cast<uint8_t>(band)) continue;
    if (!(d.flags & tef::detail::kFlagHasDefaults)) continue;
    if ((d.flags & tef::detail::kFlagFmsi) && !supportsFmsi()) continue;
    if ((d.flags & tef::detail::kFlagDr) && !supportsDigitalRadio()) continue;
    int16_t p[7];
    uint8_t n = 0;
    chipDefaults(cmd, p, n);
    const Error e = write(cmd, p, n);
    if (e != Error::Ok) return e;
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::applyRegion(tef::Region region) {
  const int16_t p[] = {static_cast<int16_t>(tef::bandPlan(region).deemphasis)};
  return write(Cmd::FmDeemphasis, p, 1);
}

// ============================================================================
// Presets
// ============================================================================

TEF668X::Error TEF668X::applyPreset(const uint8_t* preset) {
  if (preset == nullptr) return Error::InvalidArg;
  lastPresetEntry_ = 0;
  for (uint8_t entry = 0;; ++entry) {
    const uint8_t id = TEF668X_READ_U8(preset++);
    if (id == TEF_END) return Error::Ok;
    const uint8_t n = TEF668X_READ_U8(preset++);
    if (n > 7) {
      lastPresetEntry_ = entry;
      return Error::InvalidArg;
    }
    int16_t p[7];
    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t hi = TEF668X_READ_U8(preset++);
      const uint8_t lo = TEF668X_READ_U8(preset++);
      p[i] = static_cast<int16_t>((hi << 8) | lo);
    }
    const Error e = write(static_cast<Cmd>(id), p, n);
    if (e != Error::Ok) {
      lastPresetEntry_ = entry;
      return e;
    }
  }
}
