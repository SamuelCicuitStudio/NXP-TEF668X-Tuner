// Core of the TEF668X driver: bus access, start-up, checked write, tuning, reads.
// Feature helpers (L2) are in TEF668XHelpers.cpp, presets in TEF668XPresets.cpp.
#include "TEF668X.h"

#include "TEF668XPatchData.h"
#include "TEF668XPlatform.h"

using tef::detail::CmdDesc;
using tef::detail::readDesc;

namespace {

// User manual 5.6: read data needs >= 50 us setup after the request. A STOP plus
// >= 27 us idle before the read START guarantees it at every bus speed up to 400 kHz.
constexpr uint16_t kReadSetupUs = 30;

// Wire TX/RX buffer size of the core (32 on AVR, 128 on ESP32, 128 on ESP8266).
#if defined(I2C_BUFFER_LENGTH)
constexpr uint8_t kWireBuffer = (I2C_BUFFER_LENGTH > 255) ? 255 : I2C_BUFFER_LENGTH;
#elif defined(BUFFER_LENGTH)
constexpr uint8_t kWireBuffer = BUFFER_LENGTH;
#else
constexpr uint8_t kWireBuffer = 32;
#endif
constexpr uint8_t kMaxReadWords = kWireBuffer / 2;

constexpr uint8_t kTypeLithio = 9;           // Get_Identification [15:8]
constexpr uint8_t kCmdTuneTo = 1;
constexpr uint8_t kCmdRfAgc = 11;
constexpr uint16_t kAgcBeforeTuneMs = 2;     // user manual 3.4: FM Set_RFAGC >= 2 ms before an AM->FM tune
constexpr uint16_t kAgcAfterTuneMs = 11;     //                  and >= 11 ms after it

constexpr uint8_t kAppl = static_cast<uint8_t>(tef::Module::Appl);

bool supportedClock(uint32_t hz) {
  return hz == tef::kClock4MHz || hz == tef::kClock9216kHz || hz == tef::kClock12MHz || hz == tef::kClock55466kHz;
}

// GPIO function to program for the AM module when the pin is wired for an FM feature.
tef::GpioFeature amFeatureFor(tef::GpioFeature fm) {
  switch (fm) {
    case tef::GpioFeature::AgcStep: return tef::GpioFeature::High;   // AN11254 2.1: fixed high in AM
    case tef::GpioFeature::QsiDavn: return tef::GpioFeature::Qsi;    // no RDS in AM
    case tef::GpioFeature::Davn:
    case tef::GpioFeature::Rdda:
    case tef::GpioFeature::Rdcl: return tef::GpioFeature::Unused;
    default: return fm;
  }
}

}  // namespace

// ============================================================================
// Setup and health
// ============================================================================

TEF668X::Error TEF668X::begin(TwoWire& wire, const tef::BoardConfig& board) {
  if (!supportedClock(board.clockHz)) return Error::InvalidArg;
  if (board.clockHz == tef::kClock55466kHz && !board.externalClock) return Error::InvalidArg;  // no 55 MHz crystal on V102
  if (board.i2cAddress != tef::kI2cAddressLow && board.i2cAddress != tef::kI2cAddressHigh) return Error::InvalidArg;
  if (!board.fm && !board.am) return Error::InvalidArg;
  if (board.power == tef::TunerPower::EnablePin && board.powerPin < 0) return Error::InvalidArg;
  for (uint8_t i = 1; i < 3; ++i) {
    if (board.gpio[i].feature == GpioFeature::Input) return Error::InvalidArg;  // GPIO_1/2 are outputs only
  }

  wire_ = &wire;
  board_ = board;
  begun_ = true;
  ready_ = false;
  variant_ = Variant::Unknown;

  if (board_.power == tef::TunerPower::EnablePin) {
    pinMode(board_.powerPin, OUTPUT);
    digitalWrite(board_.powerPin, board_.powerActiveHigh ? HIGH : LOW);
  }
  if (board_.power != tef::TunerPower::SharedWithMcu) {
    delay(board_.powerUpDelayMs < 5 ? 5 : board_.powerUpDelayMs);  // AN11254 2.8
  }
  for (uint8_t i = 0; i < 3; ++i) {
    if (board_.gpio[i].mcuPin >= 0) pinMode(board_.gpio[i].mcuPin, INPUT_PULLUP);  // active-low TEF outputs
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::init(InitMode mode, uint16_t timeoutMs) {
  Error e = requireBegun();
  if (e != Error::Ok) return e;
  ready_ = false;
  wire_->setClock(board_.i2cHz);

  OpState st = OpState::Unknown;
  e = readOperationState(st);
  if (e != Error::Ok) return e;

  if (mode == InitMode::NoPatch && st == OpState::Boot) return Error::BadState;

  if (mode == InitMode::Patch && st != OpState::Boot) {
    // "14 00 00" is not in the user manual: community firmware uses it to go back to
    // boot state without a power cycle. Check that it worked before patching.
    const uint8_t stop[] = {0x14, 0x00, 0x00};
    e = rawWrite(stop, sizeof(stop));
    if (e != Error::Ok) return e;
    delay(250);
    e = readOperationState(st);
    if (e != Error::Ok) return e;
    if (st != OpState::Boot) return Error::BadState;
  }

  if (st == OpState::Boot) {
    wire_->setClock(board_.i2cPatchHz);
    e = uploadPatch();
    wire_->setClock(board_.i2cHz);
    if (e != Error::Ok) return e;
    const uint8_t start[] = {0x14, 0x00, 0x01};  // Start: boot -> idle (user manual 6.3.1)
    e = rawWrite(start, sizeof(start));
    if (e != Error::Ok) return e;
    e = waitState(OpState::Idle, timeoutMs);
    if (e != Error::Ok) return e;
    st = OpState::Idle;
  }

  if (st == OpState::Idle) {
    e = activateFromIdle(timeoutMs);
    if (e != Error::Ok) return e;
    st = OpState::Standby;
  }

  e = identify();
  if (e != Error::Ok) return e;
  e = applyGpioWiring();
  if (e != Error::Ok) return e;

  band_ = (st == OpState::ActiveAM) ? Band::AM : Band::FM;
  frequency_ = 0;
  ready_ = true;
  return Error::Ok;
}

TEF668X::Error TEF668X::checkAlive() {
  OpState st = OpState::Unknown;
  const Error e = readOperationState(st);
  if (e != Error::Ok) return e;
  if (st == OpState::Boot || st == OpState::Idle) {
    ready_ = false;  // power dip or clock loss: every setting is gone
    return Error::BadState;
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::requireBegun() const {
  return begun_ ? Error::Ok : Error::NotBegun;
}

TEF668X::Error TEF668X::waitState(OpState target, uint16_t timeoutMs) {
  const uint32_t t0 = millis();
  do {
    OpState st = OpState::Unknown;
    if (readOperationState(st) == Error::Ok && st == target) return Error::Ok;
    delay(5);
  } while (millis() - t0 < timeoutMs);
  return Error::Timeout;
}

TEF668X::Error TEF668X::activateFromIdle(uint16_t timeoutMs) {
  // APPL 4 Set_ReferenceClock and APPL 5 Activate are idle-state commands (3.40, 3.41).
  const int16_t clk[] = {static_cast<int16_t>(board_.clockHz >> 16), static_cast<int16_t>(board_.clockHz & 0xFFFF),
                         static_cast<int16_t>(board_.externalClock ? 1 : 0)};
  Error e = sendWords(kAppl, 4, clk, 3);
  if (e != Error::Ok) return e;
  const int16_t act[] = {1};
  e = sendWords(kAppl, 5, act, 1);
  if (e != Error::Ok) return e;
  return waitState(OpState::Standby, timeoutMs);
}

TEF668X::Error TEF668X::identify() {
  Error e = readIdentification(ident_);
  if (e != Error::Ok) return e;
  variant_ = Variant::Unknown;
  if ((ident_.device >> 8) == kTypeLithio) {
    const uint8_t v = ident_.device & 0xFF;
    if (v == 14 || v == 1 || v == 9 || v == 3) variant_ = static_cast<Variant>(v);
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::applyGpioWiring() {
  for (uint8_t pin = 0; pin < 3; ++pin) {
    const GpioFeature f = board_.gpio[pin].feature;
    if (f == GpioFeature::Unused) continue;
    if (board_.fm) {
      const int16_t p[] = {pin, static_cast<int16_t>(Module::FM), static_cast<int16_t>(f)};
      const Error e = sendWords(kAppl, 3, p, 3);
      if (e != Error::Ok) return e;
    }
    if (board_.am) {
      const int16_t p[] = {pin, static_cast<int16_t>(Module::AM), static_cast<int16_t>(amFeatureFor(f))};
      const Error e = sendWords(kAppl, 3, p, 3);
      if (e != Error::Ok) return e;
    }
  }
  return Error::Ok;
}

// ============================================================================
// Patch upload (user manual 6.2.2, Patch.c p2.24)
// ============================================================================

TEF668X::Error TEF668X::writePatchStream(const unsigned char* bytes, size_t size) {
  uint8_t chunk = board_.patchChunkBytes;
  const uint8_t maxChunk = static_cast<uint8_t>((kWireBuffer - 1) & ~1U);  // 1 byte for the 0x1B command
  if (chunk > maxChunk) chunk = maxChunk;
  chunk &= ~1U;  // split on word boundaries only
  if (chunk < 2) chunk = 2;

  for (size_t offset = 0; offset < size;) {
    size_t n = size - offset;
    if (n > chunk) n = chunk;
    wire_->beginTransmission(board_.i2cAddress);
    wire_->write(static_cast<uint8_t>(0x1B));
    for (size_t i = 0; i < n; ++i) wire_->write(TEF668X_READ_U8(bytes + offset + i));
    if (wire_->endTransmission() != 0) return Error::I2CError;
    offset += n;
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::uploadPatch() {
  if (PatchSize == 0 || LutSize == 0 || (PatchSize & 1U) || (LutSize & 1U)) return Error::PatchDataError;
  const uint8_t reset[] = {0x1C, 0x00, 0x00};
  const uint8_t selPatch[] = {0x1C, 0x00, 0x74};
  const uint8_t selLut[] = {0x1C, 0x00, 0x75};

  Error e = rawWrite(reset, 3);
  if (e == Error::Ok) e = rawWrite(selPatch, 3);
  if (e == Error::Ok) e = writePatchStream(pPatchBytes, PatchSize);
  if (e == Error::Ok) e = rawWrite(reset, 3);
  if (e == Error::Ok) e = rawWrite(selLut, 3);
  if (e == Error::Ok) e = writePatchStream(pLutBytes, LutSize);
  if (e == Error::Ok) e = rawWrite(reset, 3);
  return e;
}

// ============================================================================
// L0 - bus
// ============================================================================

TEF668X::Error TEF668X::rawWrite(const uint8_t* data, uint8_t len) {
  wire_->beginTransmission(board_.i2cAddress);
  wire_->write(data, len);
  return wire_->endTransmission() == 0 ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::sendWords(uint8_t module, uint8_t cmd, const int16_t* words, uint8_t count) {
  wire_->beginTransmission(board_.i2cAddress);
  wire_->write(module);
  wire_->write(cmd);
  wire_->write(static_cast<uint8_t>(1));  // index 1: V102 accepts no other write index
  for (uint8_t i = 0; i < count; ++i) {
    const uint16_t w = static_cast<uint16_t>(words[i]);
    wire_->write(static_cast<uint8_t>(w >> 8));
    wire_->write(static_cast<uint8_t>(w & 0xFF));
  }
  return wire_->endTransmission() == 0 ? Error::Ok : Error::I2CError;
}

TEF668X::Error TEF668X::writeWords(Module module, uint8_t cmd, const uint16_t* words, uint8_t count) {
  const Error e = requireBegun();
  if (e != Error::Ok) return e;
  if (count > 0 && words == nullptr) return Error::InvalidArg;
  return sendWords(static_cast<uint8_t>(module), cmd, reinterpret_cast<const int16_t*>(words), count);
}

TEF668X::Error TEF668X::readWords(Module module, uint8_t cmd, uint16_t* out, uint8_t count, uint8_t index) {
  const Error e = requireBegun();
  if (e != Error::Ok) return e;
  if (out == nullptr || count == 0 || count > kMaxReadWords) return Error::InvalidArg;

  wire_->beginTransmission(board_.i2cAddress);
  wire_->write(static_cast<uint8_t>(module));
  wire_->write(cmd);
  wire_->write(index);
  if (wire_->endTransmission() != 0) return Error::I2CError;
  delayMicroseconds(kReadSetupUs);

  const uint8_t bytes = static_cast<uint8_t>(count * 2);
  if (wire_->requestFrom(static_cast<int>(board_.i2cAddress), static_cast<int>(bytes)) != bytes) return Error::I2CError;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t hi = static_cast<uint8_t>(wire_->read());
    const uint8_t lo = static_cast<uint8_t>(wire_->read());
    out[i] = static_cast<uint16_t>((hi << 8) | lo);
  }
  return Error::Ok;
}

// ============================================================================
// L0 - checked write (every L1/L2/L3 write ends here)
// ============================================================================

TEF668X::Error TEF668X::write(tef::Cmd cmd, const int16_t* params, uint8_t count) {
  if (!begun_) return Error::NotBegun;
  if (static_cast<uint8_t>(cmd) >= static_cast<uint8_t>(tef::Cmd::Count)) return Error::InvalidArg;
  if (count > 0 && params == nullptr) return Error::InvalidArg;

  const CmdDesc d = readDesc(cmd);
  if (d.flags & tef::detail::kFlagIdleOnly) return Error::BadState;  // done by init()
  if (!ready_) return Error::NotReady;
  if (d.module == static_cast<uint8_t>(Module::FM) && !board_.fm) return Error::BandDisabled;
  if (d.module == static_cast<uint8_t>(Module::AM) && !board_.am) return Error::BandDisabled;
  if ((d.flags & tef::detail::kFlagFmsi) && !supportsFmsi()) return Error::UnsupportedFeature;
  if ((d.flags & tef::detail::kFlagDr) && !supportsDigitalRadio()) return Error::UnsupportedFeature;
  if (count < d.nReq || count > d.nMax) return Error::ParamCount;

  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t r = TEF668X_READ_U8(&tef::detail::kParamRange[d.param + i]);
    const uint16_t lo = TEF668X_READ_U16(&tef::detail::kRanges[r][0]);
    const uint16_t hi = TEF668X_READ_U16(&tef::detail::kRanges[r][1]);
    const bool ok = (d.signedMask & (1U << i))
                        ? (params[i] >= static_cast<int16_t>(lo) && params[i] <= static_cast<int16_t>(hi))
                        : (static_cast<uint16_t>(params[i]) >= lo && static_cast<uint16_t>(params[i]) <= hi);
    if (!ok) {
      lastErrorParam_ = i;
      return Error::OutOfRange;
    }
  }

  // User manual 3.4: keep FM Set_RFAGC >= 11 ms away from an AM->FM band change.
  if (d.module == static_cast<uint8_t>(Module::FM) && d.cmd == kCmdRfAgc) {
    const uint32_t since = millis() - bandChangeMs_;
    if (band_ == Band::FM && since < kAgcAfterTuneMs) delay(kAgcAfterTuneMs - since);
  }

  const Error e = sendWords(d.module, d.cmd, params, count);
  if (e != Error::Ok) return e;
  if (d.module == static_cast<uint8_t>(Module::FM) && d.cmd == kCmdRfAgc) fmAgcWriteMs_ = millis();
  if (shadow_ != nullptr) shadow_->record(cmd, params, count);
  return Error::Ok;
}

// ============================================================================
// Manual typed commands
// ============================================================================

TEF668X::Error TEF668X::setGpio(uint8_t pin, Band band, GpioFeature feature) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  if (pin > 2) return Error::OutOfRange;
  if (pin > 0 && feature == GpioFeature::Input) return Error::InvalidArg;
  if (!bandEnabled(band)) return Error::BandDisabled;
  const int16_t p[] = {pin, static_cast<int16_t>(band), static_cast<int16_t>(feature)};
  return sendWords(kAppl, 3, p, 3);
}

TEF668X::Error TEF668X::setDigIo(I2sSignal signal, I2sMode mode, I2sFormat format, bool master, I2sRate rate) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  if (signal == I2sSignal::Input && mode == I2sMode::Output) return Error::InvalidArg;
  if (signal == I2sSignal::Output && mode == I2sMode::Input) return Error::InvalidArg;
  const int16_t p[] = {static_cast<int16_t>(signal), static_cast<int16_t>(mode), static_cast<int16_t>(format),
                       static_cast<int16_t>(master ? 256 : 0), static_cast<int16_t>(rate)};
  return sendWords(static_cast<uint8_t>(Module::Audio), 22, p, 5);
}

// ============================================================================
// Tuning
// ============================================================================

TEF668X::Error TEF668X::tune(Band band, uint16_t frequency, TuneMode mode) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  if (!bandEnabled(band)) return Error::BandDisabled;

  if (band == Band::FM) {
    if (frequency < tef::kFmMin || frequency > tef::kFmMax) return Error::OutOfRange;
    if (mode == TuneMode::End) return Error::InvalidArg;
  } else {
    const bool lw = frequency >= tef::kLwMin && frequency <= tef::kLwMax;
    const bool mw = frequency >= tef::kMwMin && frequency <= tef::kMwMax;
    const bool sw = frequency >= tef::kSwMin && frequency <= tef::kSwMax;
    if (!lw && !mw && !sw) return Error::OutOfRange;
    if (mode != TuneMode::Preset && mode != TuneMode::Search) return Error::InvalidArg;  // AF modes are FM only
  }
  if (band != band_ && mode != TuneMode::Preset && mode != TuneMode::Search) return Error::BadState;

  const bool bandChange = band != band_;
  if (bandChange && band == Band::FM) {
    const uint32_t since = millis() - fmAgcWriteMs_;  // user manual 3.4
    if (since < kAgcBeforeTuneMs) delay(kAgcBeforeTuneMs - since);
  }

  const int16_t p[] = {static_cast<int16_t>(mode), static_cast<int16_t>(frequency)};
  const Error e = sendWords(static_cast<uint8_t>(band), kCmdTuneTo, p, 2);
  if (e != Error::Ok) return e;
  if (bandChange) bandChangeMs_ = millis();
  // AF_Update returns to the original frequency; the others move to the new one.
  if (mode != TuneMode::AfUpdate) {
    band_ = band;
    frequency_ = frequency;
  }
  return Error::Ok;
}

TEF668X::Error TEF668X::tuneFM(uint16_t frequency, TuneMode mode) { return tune(Band::FM, frequency, mode); }

TEF668X::Error TEF668X::tuneAM(uint16_t frequencyKHz, TuneMode mode) { return tune(Band::AM, frequencyKHz, mode); }

TEF668X::Error TEF668X::tuneEnd() {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  const int16_t p[] = {static_cast<int16_t>(TuneMode::End)};
  return sendWords(static_cast<uint8_t>(band_), kCmdTuneTo, p, 1);
}

// ============================================================================
// Reads
// ============================================================================

TEF668X::Error TEF668X::readOperationState(OpState& out) {
  uint16_t w = 0;
  const Error e = readWords(Module::Appl, 128, &w, 1);
  if (e != Error::Ok) return e;
  out = (w <= 4) ? static_cast<OpState>(w) : OpState::Unknown;
  return Error::Ok;
}

TEF668X::Error TEF668X::readIdentification(tef::Identification& out) {
  uint16_t w[3] = {0, 0, 0};
  const Error e = readWords(Module::Appl, 130, w, 3);
  if (e != Error::Ok) return e;
  out.device = w[0];
  out.hwVersion = w[1];
  out.swVersion = w[2];
  return Error::Ok;
}

TEF668X::Error TEF668X::readGpio0(bool& high) {
  uint16_t w = 0;
  const Error e = readWords(Module::Appl, 129, &w, 1);
  if (e == Error::Ok) high = (w & 0x0001) != 0;
  return e;
}

TEF668X::Error TEF668X::readLastWrite(tef::LastWrite& out) {
  uint16_t w[7] = {0};
  const Error e = readWords(Module::Appl, 131, w, 7);
  if (e != Error::Ok) return e;
  out.paramCount = w[0] >> 8;
  out.module = w[0] & 0xFF;
  out.cmd = w[1] >> 8;
  out.index = w[1] & 0xFF;
  for (uint8_t i = 0; i < 5; ++i) out.params[i] = w[2 + i];
  return Error::Ok;
}

TEF668X::Error TEF668X::readQuality(tef::Quality& out, bool release) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  uint16_t w[7] = {0};
  const Error e = readWords(currentModule(), release ? 129 : 128, w, 7);
  if (e != Error::Ok) return e;
  out.band = band_;
  out.status = w[0];
  out.level = static_cast<int16_t>(w[1]);
  out.noise = w[2];
  out.word4 = w[3];
  out.offset = static_cast<int16_t>(w[4]);
  out.bandwidth = w[5];
  out.modulation = w[6];
  return Error::Ok;
}

TEF668X::Error TEF668X::readAgc(tef::AgcStatus& out) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  uint16_t w[2] = {0, 0};
  const Error e = readWords(currentModule(), 132, w, 2);
  if (e != Error::Ok) return e;
  out.inputAttenuation = w[0];
  out.feedbackAttenuation = w[1];
  return Error::Ok;
}

TEF668X::Error TEF668X::readSignal(tef::SignalStatus& out) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  uint16_t w = 0;
  const Error e = readWords(currentModule(), 133, &w, 1);
  if (e != Error::Ok) return e;
  out.stereo = (w & 0x8000) != 0;
  out.digital = (w & 0x4000) != 0;
  return Error::Ok;
}

TEF668X::Error TEF668X::readProcessing(tef::ProcessingStatus& out) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  uint16_t w[6] = {0};
  const uint8_t n = (band_ == Band::FM && supportsFmsi()) ? 6 : 4;
  const Error e = readWords(currentModule(), 134, w, n);
  if (e != Error::Ok) return e;
  out.softmute = w[0];
  out.highcut = w[1];
  out.stereo = w[2];
  out.stHiBlend = w[3];
  out.fmsiBand[0] = w[4] >> 8;
  out.fmsiBand[1] = w[4] & 0xFF;
  out.fmsiBand[2] = w[5] >> 8;
  out.fmsiBand[3] = w[5] & 0xFF;
  return Error::Ok;
}

TEF668X::Error TEF668X::readDrSampleRate(uint16_t& out) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  if (!supportsDigitalRadio()) return Error::UnsupportedFeature;
  return readWords(currentModule(), 135, &out, 1);
}

TEF668X::Error TEF668X::readRds(tef::RdsGroup& out, bool release) {
  if (!ready_) return begun_ ? Error::NotReady : Error::NotBegun;
  if (!board_.fm) return Error::BandDisabled;
  if (band_ != Band::FM) return Error::BadState;
  uint16_t w[6] = {0};
  const Error e = readWords(Module::FM, release ? 131 : 130, w, 6);
  if (e != Error::Ok) return e;
  out.status = w[0];
  for (uint8_t i = 0; i < 4; ++i) out.block[i] = w[1 + i];
  out.errors = w[5];
  return Error::Ok;
}
