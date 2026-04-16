#include "TEF668X.h"

#include "TEF668XPatchData.h"

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

namespace {
static inline uint8_t readPatchByte(const unsigned char* p) {
#if defined(__AVR__)
  return pgm_read_byte(p);
#else
  return *p;
#endif
}
} // namespace

bool TEF668X::begin(TwoWire& wire, uint8_t i2cAddress) {
  wire_ = &wire;
  addr_ = i2cAddress;
  begun_ = true;
  ready_ = false;
  activeVariant_ = Variant::Unknown;
  detectedVariant_ = Variant::Unknown;
  variantMismatch_ = false;
  return true;
}

void TEF668X::setI2CSpeeds(uint32_t patchHz, uint32_t runHz) {
  i2cPatchHz_ = patchHz;
  i2cRunHz_ = runHz;
}

void TEF668X::setPatchChunkSize(size_t chunkBytes) {
  if (chunkBytes < 2) {
    patchChunkBytes_ = 2;
    return;
  }
  patchChunkBytes_ = (chunkBytes & 1U) ? (chunkBytes - 1U) : chunkBytes;
}

void TEF668X::setVariantPolicy(VariantPolicy policy) {
  variantPolicy_ = policy;
}

TEF668X::VariantPolicy TEF668X::getVariantPolicy() const {
  return variantPolicy_;
}

TEF668X::Variant TEF668X::policyToVariant(VariantPolicy policy) {
  switch (policy) {
    case VariantPolicy::ForceTEF6686: return Variant::TEF6686;
    case VariantPolicy::ForceTEF6687: return Variant::TEF6687;
    case VariantPolicy::ForceTEF6688: return Variant::TEF6688;
    case VariantPolicy::ForceTEF6689: return Variant::TEF6689;
    case VariantPolicy::AutoDetect:
    default: return Variant::Unknown;
  }
}

TEF668X::Error TEF668X::writeRaw(const uint8_t* data, size_t len) {
  if (!begun_) return Error::NotBegun;
  if (!data || len == 0) return Error::InvalidArg;

  wire_->beginTransmission(addr_);
  wire_->write(data, len);
  return (wire_->endTransmission() == 0) ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::writeWordsU16(uint8_t module, uint8_t cmd, uint8_t index, const uint16_t* words, uint8_t wordCount) {
  if (!begun_) return Error::NotBegun;
  if (wordCount > 0 && words == nullptr) return Error::InvalidArg;

  wire_->beginTransmission(addr_);
  wire_->write(module);
  wire_->write(cmd);
  wire_->write(index);
  for (uint8_t i = 0; i < wordCount; ++i) {
    const uint16_t w = words[i];
    wire_->write(static_cast<uint8_t>(w >> 8));
    wire_->write(static_cast<uint8_t>(w & 0xFF));
  }
  return (wire_->endTransmission() == 0) ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::writeWordsS16(uint8_t module, uint8_t cmd, uint8_t index, const int16_t* words, uint8_t wordCount) {
  if (!begun_) return Error::NotBegun;
  if (wordCount > 0 && words == nullptr) return Error::InvalidArg;

  wire_->beginTransmission(addr_);
  wire_->write(module);
  wire_->write(cmd);
  wire_->write(index);
  for (uint8_t i = 0; i < wordCount; ++i) {
    const uint16_t w = static_cast<uint16_t>(words[i]);
    wire_->write(static_cast<uint8_t>(w >> 8));
    wire_->write(static_cast<uint8_t>(w & 0xFF));
  }
  return (wire_->endTransmission() == 0) ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::readWords(uint8_t module, uint8_t cmd, uint8_t index, uint16_t* out, uint8_t wordCount) {
  if (!begun_) return Error::NotBegun;
  if (!out || wordCount == 0) return Error::InvalidArg;

  wire_->beginTransmission(addr_);
  wire_->write(module);
  wire_->write(cmd);
  wire_->write(index);
  if (wire_->endTransmission(false) != 0) return Error::I2CError;

  const uint8_t byteCount = static_cast<uint8_t>(wordCount * 2U);
  const uint8_t got = wire_->requestFrom(static_cast<int>(addr_), static_cast<int>(byteCount));
  if (got != byteCount) return Error::I2CError;

  for (uint8_t i = 0; i < wordCount; ++i) {
    const uint8_t hi = wire_->read();
    const uint8_t lo = wire_->read();
    out[i] = static_cast<uint16_t>((hi << 8) | lo);
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::stopBoot() {
  const uint8_t buf[3] = {0x14, 0x00, 0x00};
  return writeRaw(buf, sizeof(buf));
}

TEF668X::Error TEF668X::startBoot() {
  const uint8_t buf[3] = {0x14, 0x00, 0x01};
  return writeRaw(buf, sizeof(buf));
}

TEF668X::Error TEF668X::activate() {
  const uint16_t p[] = {1};
  return writeWordsU16(0x40, 0x05, 0x01, p, 1);
}

TEF668X::Error TEF668X::setReferenceClock(uint32_t frequencyHz, bool externalClock) {
  const uint16_t p[] = {
    static_cast<uint16_t>((frequencyHz >> 16) & 0xFFFFU),
    static_cast<uint16_t>(frequencyHz & 0xFFFFU),
    static_cast<uint16_t>(externalClock ? 1U : 0U)
  };
  return writeWordsU16(0x40, 0x04, 0x01, p, 3);
}

TEF668X::Error TEF668X::getOperationStatus(uint16_t& status) {
  return readWords(0x40, 0x80, 0x01, &status, 1);
}

TEF668X::Error TEF668X::waitForState(uint16_t target, uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  while ((millis() - t0) < timeoutMs) {
    uint16_t st = 0;
    const Error e = getOperationStatus(st);
    if (e == Error::Ok && st == target) return Error::Ok;
    delay(10);
  }
  return Error::Timeout;
}

TEF668X::Error TEF668X::writePatchChunk(const unsigned char* bytes, size_t offset, size_t len) {
  if (!bytes || len == 0) return Error::InvalidArg;

  wire_->beginTransmission(addr_);
  wire_->write(static_cast<uint8_t>(0x1B));
  for (size_t i = 0; i < len; ++i) {
    wire_->write(readPatchByte(&bytes[offset + i]));
  }
  return (wire_->endTransmission() == 0) ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::uploadPatchAndLut() {
  if ((PatchSize == 0) || (LutSize == 0)) return Error::PatchDataError;
  if ((PatchSize & 1U) || (LutSize & 1U)) return Error::PatchDataError;

  const auto write1C = [this](uint16_t value) {
    const uint8_t b[3] = {0x1C, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    return writeRaw(b, sizeof(b));
  };

  Error e = write1C(0x0000);
  if (e != Error::Ok) return e;
  e = write1C(0x0074);
  if (e != Error::Ok) return e;

  for (size_t offset = 0; offset < PatchSize; ) {
    size_t remaining = PatchSize - offset;
    size_t step = (patchChunkBytes_ < remaining) ? patchChunkBytes_ : remaining;
    if (step & 1U) step -= 1U;
    if (step == 0) return Error::PatchDataError;
    e = writePatchChunk(pPatchBytes, offset, step);
    if (e != Error::Ok) return e;
    offset += step;
  }

  e = write1C(0x0000);
  if (e != Error::Ok) return e;
  e = write1C(0x0075);
  if (e != Error::Ok) return e;

  for (size_t offset = 0; offset < LutSize; ) {
    size_t remaining = LutSize - offset;
    size_t step = (patchChunkBytes_ < remaining) ? patchChunkBytes_ : remaining;
    if (step & 1U) step -= 1U;
    if (step == 0) return Error::PatchDataError;
    e = writePatchChunk(pLutBytes, offset, step);
    if (e != Error::Ok) return e;
    offset += step;
  }

  return write1C(0x0000);
}

TEF668X::Error TEF668X::getIdentification(Identification& out) {
  uint16_t words[3] = {0, 0, 0};
  const Error e = readWords(0x40, 0x82, 0x01, words, 3);
  if (e != Error::Ok) return e;
  out.device = words[0];
  out.hwVersion = words[1];
  out.swVersion = words[2];
  return Error::Ok;
}

TEF668X::Error TEF668X::detectVariant() {
  Error e = getIdentification(identification_);
  if (e != Error::Ok) {
    detectedVariant_ = Variant::Unknown;
    activeVariant_ = Variant::Unknown;
    variantMismatch_ = false;
    return e;
  }
  const uint8_t variantByte = static_cast<uint8_t>(identification_.device & 0xFFU);
  Variant parsed = Variant::Unknown;
  switch (variantByte) {
    case static_cast<uint8_t>(Variant::TEF6686):
      parsed = Variant::TEF6686;
      break;
    case static_cast<uint8_t>(Variant::TEF6687):
      parsed = Variant::TEF6687;
      break;
    case static_cast<uint8_t>(Variant::TEF6688):
      parsed = Variant::TEF6688;
      break;
    case static_cast<uint8_t>(Variant::TEF6689):
      parsed = Variant::TEF6689;
      break;
    default:
      parsed = Variant::Unknown;
      break;
  }
  detectedVariant_ = parsed;

  const Variant forced = policyToVariant(variantPolicy_);
  if (variantPolicy_ == VariantPolicy::AutoDetect) {
    activeVariant_ = detectedVariant_;
    variantMismatch_ = false;
  } else {
    activeVariant_ = forced;
    variantMismatch_ = (detectedVariant_ != Variant::Unknown) && (forced != detectedVariant_);
  }
  return Error::Ok;
}

TEF668X::Variant TEF668X::getActiveVariant() const {
  return activeVariant_;
}

TEF668X::Variant TEF668X::getDetectedVariant() const {
  return detectedVariant_;
}

bool TEF668X::hasVariantMismatch() const {
  return variantMismatch_;
}

bool TEF668X::supportsFmsi() const {
  return activeVariant_ == Variant::TEF6687 || activeVariant_ == Variant::TEF6689;
}

bool TEF668X::supportsDigitalRadio() const {
  return activeVariant_ == Variant::TEF6688 || activeVariant_ == Variant::TEF6689;
}

TEF668X::Error TEF668X::requireFmsiFeature() const {
  return supportsFmsi() ? Error::Ok : Error::UnsupportedFeature;
}

TEF668X::Error TEF668X::requireDigitalRadioFeature() const {
  return supportsDigitalRadio() ? Error::Ok : Error::UnsupportedFeature;
}

bool TEF668X::isReady() const {
  return ready_;
}

TEF668X::Error TEF668X::initSoft(uint32_t crystalHz, bool externalClock, uint32_t timeoutMs) {
  if (!begun_) return Error::NotBegun;

  ready_ = false;
  wire_->setClock(i2cRunHz_);

  Error e = stopBoot();
  if (e != Error::Ok) return e;
  delay(150);

  e = startBoot();
  if (e != Error::Ok) return e;

  e = waitForState(static_cast<uint16_t>(OpState::Idle), timeoutMs);
  if (e != Error::Ok) return e;

  e = setReferenceClock(crystalHz, externalClock);
  if (e != Error::Ok) return e;

  e = activate();
  if (e != Error::Ok) return e;

  e = waitForState(static_cast<uint16_t>(OpState::ActiveStandby), timeoutMs);
  if (e != Error::Ok) return e;

  e = detectVariant();
  if (e != Error::Ok) return e;

  ready_ = true;
  return Error::Ok;
}

TEF668X::Error TEF668X::initHard(uint32_t crystalHz, bool externalClock, uint32_t timeoutMs) {
  if (!begun_) return Error::NotBegun;

  ready_ = false;
  wire_->setClock(i2cPatchHz_);

  Error e = stopBoot();
  if (e != Error::Ok) return e;
  delay(250);

  e = uploadPatchAndLut();
  if (e != Error::Ok) return e;

  e = startBoot();
  if (e != Error::Ok) return e;

  e = waitForState(static_cast<uint16_t>(OpState::Idle), timeoutMs);
  if (e != Error::Ok) return e;

  e = setReferenceClock(crystalHz, externalClock);
  if (e != Error::Ok) return e;

  e = activate();
  if (e != Error::Ok) return e;

  e = waitForState(static_cast<uint16_t>(OpState::ActiveStandby), timeoutMs);
  if (e != Error::Ok) return e;

  wire_->setClock(i2cRunHz_);
  e = detectVariant();
  if (e != Error::Ok) return e;

  ready_ = true;
  return Error::Ok;
}

TEF668X::Error TEF668X::initAuto(uint32_t crystalHz, bool externalClock) {
  Error e = initSoft(crystalHz, externalClock, 600);
  if (e == Error::Ok) return Error::Ok;

  delay(400);
  e = initSoft(crystalHz, externalClock, 600);
  if (e == Error::Ok) return Error::Ok;

  return initHard(crystalHz, externalClock, 1200);
}

TEF668X::Error TEF668X::applyFmDefaultProfile(int16_t volume_x10_dB) {
  return applyFmCarProfile(volume_x10_dB);
}

bool TEF668X::isFmFrequencyValid(uint16_t frequency_kHz) {
  return frequency_kHz >= 6500U && frequency_kHz <= 10800U;
}

bool TEF668X::isAmFrequencyValid(uint16_t frequency_kHz) {
  const bool lw = frequency_kHz >= 144U && frequency_kHz <= 288U;
  const bool mw = frequency_kHz >= 522U && frequency_kHz <= 1710U;
  const bool sw = frequency_kHz >= 2300U && frequency_kHz <= 27000U;
  return lw || mw || sw;
}

TEF668X::Error TEF668X::tuneFmPreset(uint16_t frequency_kHz) {
  if (!isFmFrequencyValid(frequency_kHz)) return Error::InvalidArg;
  const uint16_t p[] = {1, frequency_kHz};
  return writeWordsU16(0x20, 0x01, 0x01, p, 2);
}

TEF668X::Error TEF668X::tuneFmSearch(uint16_t frequency_kHz) {
  if (!isFmFrequencyValid(frequency_kHz)) return Error::InvalidArg;
  const uint16_t p[] = {2, frequency_kHz};
  return writeWordsU16(0x20, 0x01, 0x01, p, 2);
}

TEF668X::Error TEF668X::tuneAmPreset(uint16_t frequency_kHz) {
  if (!isAmFrequencyValid(frequency_kHz)) return Error::InvalidArg;
  const uint16_t p[] = {1, frequency_kHz};
  return writeWordsU16(0x21, 0x01, 0x01, p, 2);
}

TEF668X::Error TEF668X::releaseSearchOrCheckMuteFmAm() {
  uint16_t op = 0;
  Error e = getOperationStatus(op);
  if (e != Error::Ok) return e;

  const uint16_t p[] = {7};
  if (op == static_cast<uint16_t>(OpState::ActiveAm)) {
    return writeWordsU16(0x21, 0x01, 0x01, p, 1);
  }
  return writeWordsU16(0x20, 0x01, 0x01, p, 1);
}

TEF668X::Error TEF668X::setVolumeDb10(int16_t volume_x10_dB) {
  const int16_t p[] = {volume_x10_dB};
  return writeWordsS16(0x30, 0x0A, 0x01, p, 1);
}

TEF668X::Error TEF668X::setMute(bool mute) {
  const uint16_t p[] = {static_cast<uint16_t>(mute ? 1U : 0U)};
  return writeWordsU16(0x30, 0x0B, 0x01, p, 1);
}

TEF668X::Error TEF668X::getQualityFm(QualityData& out) {
  uint16_t w[7] = {0, 0, 0, 0, 0, 0, 0};
  Error e = readWords(0x20, 0x81, 0x01, w, 7);
  if (e != Error::Ok) return e;

  out.status = w[0];
  out.level_x10_dBuV = static_cast<int16_t>(w[1]);
  out.usn_x10_pct = w[2];
  out.wam_x10_pct = w[3];
  out.offset_x10_kHz = static_cast<int16_t>(w[4]);
  out.bandwidth_x10_kHz = w[5];
  out.modulation_x10_pct = w[6];
  return Error::Ok;
}

TEF668X::Error TEF668X::getSignalStatusFm(uint16_t& status) {
  return readWords(0x20, 0x85, 0x01, &status, 1);
}

TEF668X::Error TEF668X::getProcessingStatusFm(ProcessingStatusFm& out) {
  uint16_t w[4] = {0, 0, 0, 0};
  Error e = readWords(0x20, 0x86, 0x01, w, 4);
  if (e != Error::Ok) return e;
  out.softmute_x10_pct = w[0];
  out.highcut_x10_pct = w[1];
  out.stereo_x10_pct = w[2];
  out.stHiBlend_x10_pct = w[3];
  return Error::Ok;
}

TEF668X::Error TEF668X::getInterfaceStatus(uint16_t& sampleRate_kHz_x1) {
  Error e = requireDigitalRadioFeature();
  if (e != Error::Ok) return e;
  return readWords(0x20, 0x87, 0x01, &sampleRate_kHz_x1, 1);
}

TEF668X::Error TEF668X::setStereoImprovement(uint16_t mode) {
  Error e = requireFmsiFeature();
  if (e != Error::Ok) return e;
  const uint16_t p[] = {mode};
  return writeWordsU16(0x20, 0x20, 0x01, p, 1);
}

TEF668X::Error TEF668X::setStBandBlendTime(uint16_t attack_ms, uint16_t decay_ms) {
  Error e = requireFmsiFeature();
  if (e != Error::Ok) return e;
  const uint16_t p[] = {attack_ms, decay_ms};
  return writeWordsU16(0x20, 0x5A, 0x01, p, 2);
}

TEF668X::Error TEF668X::setStBandBlendGain(uint16_t band1, uint16_t band2, uint16_t band3, uint16_t band4) {
  Error e = requireFmsiFeature();
  if (e != Error::Ok) return e;
  const uint16_t p[] = {band1, band2, band3, band4};
  return writeWordsU16(0x20, 0x5B, 0x01, p, 4);
}

TEF668X::Error TEF668X::setStBandBlendBias(int16_t band1, int16_t band2, int16_t band3, int16_t band4) {
  Error e = requireFmsiFeature();
  if (e != Error::Ok) return e;
  const int16_t p[] = {band1, band2, band3, band4};
  return writeWordsS16(0x20, 0x5C, 0x01, p, 4);
}

TEF668X::Error TEF668X::setDigitalRadio(uint16_t mode) {
  Error e = requireDigitalRadioFeature();
  if (e != Error::Ok) return e;
  const uint16_t p[] = {mode};
  return writeWordsU16(0x20, 0x1E, 0x01, p, 1);
}

TEF668X::Error TEF668X::setDrBlend(uint16_t mode, uint16_t inTime_ms, uint16_t outTime_ms, int16_t gain_x10_dB) {
  Error e = requireDigitalRadioFeature();
  if (e != Error::Ok) return e;
  const int16_t p[] = {
    static_cast<int16_t>(mode),
    static_cast<int16_t>(inTime_ms),
    static_cast<int16_t>(outTime_ms),
    gain_x10_dB
  };
  return writeWordsS16(0x20, 0x53, 0x01, p, 4);
}

TEF668X::Error TEF668X::setDrOptions(uint16_t sampleRate, uint16_t mode, uint16_t format) {
  Error e = requireDigitalRadioFeature();
  if (e != Error::Ok) return e;
  const uint16_t p[] = {sampleRate, mode, format};
  return writeWordsU16(0x20, 0x54, 0x01, p, 3);
}

TEF668X::Error TEF668X::applyFmCarProfile(int16_t volume_x10_dB) {
  if (!begun_) return Error::NotBegun;

  const auto wU = [this](uint8_t cmd, const uint16_t* p, uint8_t n) {
    return writeWordsU16(0x20, cmd, 0x01, p, n);
  };

  Error e = Error::Ok;

  { const uint16_t p[] = {500}; e = wU(0x1F, p, 1); if (e != Error::Ok) return e; }                   // Deemphasis 50us
  { const uint16_t p[] = {1, 2360, 1000, 1000}; e = wU(0x0A, p, 4); if (e != Error::Ok) return e; }   // Bandwidth auto
  { const uint16_t p[] = {890, 0}; e = wU(0x0B, p, 2); if (e != Error::Ok) return e; }                 // RF AGC
  { const uint16_t p[] = {1}; e = wU(0x14, p, 1); if (e != Error::Ok) return e; }                      // MPH suppression
  { const uint16_t p[] = {1}; e = wU(0x16, p, 1); if (e != Error::Ok) return e; }                      // Channel EQ
  { const uint16_t p[] = {1, 1000}; e = wU(0x17, p, 2); if (e != Error::Ok) return e; }                // Noise blanker

  { const uint16_t p[] = {120, 500, 10, 20}; e = wU(0x28, p, 4); if (e != Error::Ok) return e; }      // SoftMute time
  { const uint16_t p[] = {3, 150, 220}; e = wU(0x2A, p, 3); if (e != Error::Ok) return e; }            // SoftMute level
  { const uint16_t p[] = {3, 500, 1000}; e = wU(0x2B, p, 3); if (e != Error::Ok) return e; }           // SoftMute noise
  { const uint16_t p[] = {3, 500, 1000}; e = wU(0x2C, p, 3); if (e != Error::Ok) return e; }           // SoftMute mph
  { const uint16_t p[] = {1, 200}; e = wU(0x2D, p, 2); if (e != Error::Ok) return e; }                 // SoftMute max

  { const uint16_t p[] = {200, 2000, 10, 80}; e = wU(0x32, p, 4); if (e != Error::Ok) return e; }     // HighCut time
  { const uint16_t p[] = {3, 360, 300}; e = wU(0x34, p, 3); if (e != Error::Ok) return e; }            // HighCut level
  { const uint16_t p[] = {3, 150, 200}; e = wU(0x35, p, 3); if (e != Error::Ok) return e; }            // HighCut noise
  { const uint16_t p[] = {3, 120, 160}; e = wU(0x36, p, 3); if (e != Error::Ok) return e; }            // HighCut mph
  { const uint16_t p[] = {1, 2400}; e = wU(0x37, p, 2); if (e != Error::Ok) return e; }                // HighCut max
  { const uint16_t p[] = {1, 10000}; e = wU(0x38, p, 2); if (e != Error::Ok) return e; }               // HighCut min
  { const uint16_t p[] = {1, 100}; e = wU(0x39, p, 2); if (e != Error::Ok) return e; }                 // LowCut max
  { const uint16_t p[] = {1, 20}; e = wU(0x3A, p, 2); if (e != Error::Ok) return e; }                  // LowCut min
  { const uint16_t p[] = {1}; e = wU(0x3B, p, 1); if (e != Error::Ok) return e; }                      // HighCut options

  { const uint16_t p[] = {200, 4000, 20, 80}; e = wU(0x3C, p, 4); if (e != Error::Ok) return e; }     // Stereo time
  { const uint16_t p[] = {3, 460, 240}; e = wU(0x3E, p, 3); if (e != Error::Ok) return e; }            // Stereo level
  { const uint16_t p[] = {3, 120, 160}; e = wU(0x3F, p, 3); if (e != Error::Ok) return e; }            // Stereo noise
  { const uint16_t p[] = {3, 100, 150}; e = wU(0x40, p, 3); if (e != Error::Ok) return e; }            // Stereo mph
  { const uint16_t p[] = {1}; e = wU(0x41, p, 1); if (e != Error::Ok) return e; }                      // Stereo max

  { const uint16_t p[] = {120, 500, 20, 80}; e = wU(0x46, p, 4); if (e != Error::Ok) return e; }      // StHiBlend time
  { const uint16_t p[] = {3, 600, 240}; e = wU(0x48, p, 3); if (e != Error::Ok) return e; }            // StHiBlend level
  { const uint16_t p[] = {3, 80, 140}; e = wU(0x49, p, 3); if (e != Error::Ok) return e; }             // StHiBlend noise
  { const uint16_t p[] = {3, 80, 140}; e = wU(0x4A, p, 3); if (e != Error::Ok) return e; }             // StHiBlend mph
  { const uint16_t p[] = {1, 3000}; e = wU(0x4B, p, 2); if (e != Error::Ok) return e; }                // StHiBlend max

  { const uint16_t p[] = {1, 2, 0}; e = wU(0x51, p, 3); if (e != Error::Ok) return e; }                // RDS
  { const int16_t p[] = {0}; e = writeWordsS16(0x20, 0x50, 0x01, p, 1); if (e != Error::Ok) return e; } // FM scaler
  return setVolumeDb10(volume_x10_dB);
}
