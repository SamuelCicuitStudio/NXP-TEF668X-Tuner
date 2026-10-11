#pragma once
/**
 * @file TEF668XTypes.h
 * @brief Every named value of the TEF668X: errors, bands, option enums, constants,
 *        board description and the structures returned by the read functions.
 *
 * Design rule: no magic numbers in user code. Each value the user manual defines
 * has a name here (enum class = type checked, costs nothing at run time).
 * Ranges and chip defaults of every command parameter are generated in
 * TEF668XCommands.h (for example tef::FmSoftmuteLevel::kStartDefault).
 */
#include <stdint.h>

namespace tef {

// ============================================================================
// Results and identities
// ============================================================================

/// Result of every library call. Nothing throws, nothing is silently clamped.
enum class Error : uint8_t {
  Ok = 0,             ///< done
  NotBegun,           ///< begin() was not called
  NotReady,           ///< init() has not completed
  I2CError,           ///< the chip did not acknowledge / short read
  Timeout,            ///< a state change took too long
  InvalidArg,         ///< argument not valid for this function (e.g. unknown preset entry)
  OutOfRange,         ///< parameter outside the user-manual range: see TEF668X::lastErrorParam()
  ParamCount,         ///< wrong number of parameters for the command
  UnsupportedFeature, ///< the detected variant has no such feature (FMSI / digital radio)
  BandDisabled,       ///< the band is disabled in BoardConfig (fm / am = false)
  BadState,           ///< command not allowed in the current device state
  PatchDataError      ///< embedded patch/LUT data is invalid
};

/// Radio band. The value is the TEF module number used on the bus.
enum class Band : uint8_t { FM = 32, AM = 33 };

/// TEF module numbers (user manual 2.1).
enum class Module : uint8_t { FM = 32, AM = 33, Audio = 48, Appl = 64 };

/// Chip variant (low byte of Get_Identification).
enum class Variant : uint8_t {
  Unknown = 0xFF,
  TEF6686 = 14,  ///< base
  TEF6687 = 1,   ///< + FMSI
  TEF6688 = 9,   ///< + digital radio
  TEF6689 = 3    ///< + FMSI + digital radio
};

/// Device operation state (APPL Get_Operation_Status, user manual 4.7).
enum class OpState : uint8_t { Boot = 0, Idle = 1, Standby = 2, ActiveFM = 3, ActiveAM = 4, Unknown = 0xFF };

// ============================================================================
// Hardware description (what is on the board) - passed to TEF668X::begin()
// ============================================================================

/// Supported reference clocks (user manual 3.40, AN11254 2.7).
constexpr uint32_t kClock4MHz = 4000000UL;       ///< crystal; not usable with digital radio
constexpr uint32_t kClock9216kHz = 9216000UL;    ///< crystal, chip default
constexpr uint32_t kClock12MHz = 12000000UL;     ///< crystal
constexpr uint32_t kClock55466kHz = 55466670UL;  ///< external clock only (V101/V102)

/// I2C address: GPIO_2 low at power-up -> 0x64, high -> 0x65 (user manual 5.1).
constexpr uint8_t kI2cAddressLow = 0x64;
constexpr uint8_t kI2cAddressHigh = 0x65;

/// How the tuner is powered (AN11254 2.8: no I2C traffic during the first 5 ms).
enum class TunerPower : uint8_t {
  SharedWithMcu,  ///< same supply as the MCU: nothing to do
  Switched,       ///< separate supply switched by the user: begin() waits powerUpDelayMs
  EnablePin       ///< the library switches it with BoardConfig::powerPin, then waits
};

/// Function of a TEF GPIO pin (user manual 3.39). Value = API feature code.
enum class GpioFeature : uint16_t {
  Unused = 0,     ///< not used / not connected
  Input = 1,      ///< general input (only GPIO_0 is advised as input)
  Low = 2,        ///< output driven low
  High = 3,       ///< output driven high
  Davn = 257,     ///< RDS data-available, active low (FM)
  Qsi = 258,      ///< quality status interrupt, active low
  QsiDavn = 259,  ///< Qsi OR Davn, active low
  Rdda = 260,     ///< legacy RDS data (not for new designs)
  Rdcl = 261,     ///< legacy RDS clock (not for new designs)
  AgcStep = 262   ///< external FM AGC step (AN11254 2.1); driven high in AM
};

/// What one TEF GPIO is wired to.
struct GpioWiring {
  GpioFeature feature = GpioFeature::Unused;  ///< function to program at init
  int8_t mcuPin = -1;                         ///< MCU input wired to it (Davn/Qsi interrupt), -1 = none
};

/**
 * @brief Everything the library must know about the board. Defaults = the common
 *        TEF6686 module: 9.216 MHz crystal, address 0x64, FM and AM, shared supply.
 */
struct BoardConfig {
  uint32_t clockHz = kClock9216kHz;          ///< kClock4MHz / kClock9216kHz / kClock12MHz / kClock55466kHz
  bool externalClock = false;                ///< true = clock signal on XTAL_IN instead of a crystal
  uint8_t i2cAddress = kI2cAddressLow;       ///< kI2cAddressLow / kI2cAddressHigh
  bool fm = true;                            ///< FM used on this product
  bool am = true;                            ///< AM (LW/MW/SW) used on this product
  TunerPower power = TunerPower::SharedWithMcu;
  int8_t powerPin = -1;                      ///< for TunerPower::EnablePin
  bool powerActiveHigh = true;               ///< level that switches the tuner on
  uint16_t powerUpDelayMs = 10;              ///< wait after power-up before any I2C (>= 5 ms)
  GpioWiring gpio[3];                        ///< TEF GPIO_0, GPIO_1, GPIO_2
  uint32_t i2cHz = 100000UL;                 ///< bus speed in operation, up to 400000 (reads are timed for it)
  uint32_t i2cPatchHz = 100000UL;            ///< bus speed while uploading the patch, up to 400000
  uint8_t patchChunkBytes = 24;              ///< bytes per patch write (clamped to the Wire buffer)
};

// ============================================================================
// Tuning and regions
// ============================================================================

/// Tune_To action (user manual 3.1).
enum class TuneMode : uint8_t {
  Preset = 1,    ///< tune with a short mute, settles fast
  Search = 2,    ///< tune and stay muted (judge quality, then tuneEnd() or Preset)
  AfUpdate = 3,  ///< FM: measure an alternative frequency inaudibly and come back
  Jump = 4,      ///< FM: move to an alternative frequency with inaudible mute
  Check = 5,     ///< FM: move to an alternative frequency and stay muted
  End = 7        ///< release the mute of Search / Check
};

/// FM de-emphasis (user manual 3.14).
enum class Deemphasis : uint16_t { Off = 0, Us50 = 500, Us75 = 750 };

/// Broadcast region: band limits, steps and de-emphasis.
enum class Region : uint8_t { Europe, USA, Japan, Oirt };

/// Band plan of a region (frequencies in tuning units).
struct BandPlan {
  uint16_t fmMin, fmMax, fmStep;  ///< FM, 10 kHz units (8750 = 87.5 MHz)
  uint16_t amMin, amMax, amStep;  ///< MW, kHz
  Deemphasis deemphasis;
};

/// Band plan of a region. Europe 87.5-108/100 kHz, MW 531-1602/9; USA 87.5-107.9/200 kHz,
/// MW 530-1700/10, 75 us; Japan 76-95/100 kHz, MW 531-1602/9; OIRT 65.9-74/30 kHz.
BandPlan bandPlan(Region r);

/// Tuning range limits (user manual 3.1).
constexpr uint16_t kFmMin = 6500, kFmMax = 10800;   ///< 10 kHz units
constexpr uint16_t kLwMin = 144, kLwMax = 288;      ///< kHz
constexpr uint16_t kMwMin = 522, kMwMax = 1710;     ///< kHz
constexpr uint16_t kSwMin = 2300, kSwMax = 27000;   ///< kHz

// ============================================================================
// Option enums used by the typed commands (values = API codes)
// ============================================================================

/// Weak-signal detector timer (SoftMute/HighCut/Stereo/StHiBlend _Level/_Noise/_Mph).
enum class WshMode : uint8_t { Off = 0, Fast = 1, Slow = 2, Dual = 3 };
/// Restart control (RDS decoder, AM co-channel detector).
enum class Restart : uint8_t { None = 0, Now = 1, AfterTune = 2 };
/// HighCut filter characteristic (FM cmd 59).
enum class HighcutFilter : uint8_t { Iir = 1, Deemphasis = 2, Fir = 3 };
/// Stereo minimum (FM cmd 66).
enum class StereoMin : uint8_t { Off = 0, Limit = 1, ForcedMono = 2 };
/// RDS mode (FM cmd 81).
enum class RdsMode : uint8_t { Off = 0, Decoder = 1, Demodulator = 2 };
/// RDS pin output (FM cmd 81).
enum class RdsPin : uint8_t { None = 0, Davn = 2, Legacy2Wire = 4 };
/// Quality status pin output (cmd 82).
enum class QsiPin : uint8_t { None = 0, Gpio = 2 };
/// Digital radio blend (cmd 83).
enum class DrBlend : uint8_t { Pin = 0, PinInverted = 1, ForceDigital = 2, ForceAnalog = 3 };
/// Special DAC output (cmd 85).
enum class AnaOutSpecial : uint8_t { Normal = 0, Mpx = 1, DigitalAnalog = 2 };
/// Audio sources (AUDIO cmd 12/13).
enum class AudioSource : uint8_t { Radio = 0, AnalogRadio = 4, I2sIn = 32, AudioProcessor = 224, SineGenerator = 240 };
/// Audio outputs (AUDIO cmd 13/21).
enum class AudioOutput : uint8_t { I2sOut = 33, Dac = 128 };
/// I2S signals (AUDIO cmd 22/23).
enum class I2sSignal : uint8_t { Input = 32, Output = 33 };
/// I2S mode (AUDIO cmd 22).
enum class I2sMode : uint8_t { Off = 0, Input = 1, Output = 2 };
/// I2S format (AUDIO cmd 22).
enum class I2sFormat : uint16_t { I2s16 = 16, I2s32 = 32, Lsb16 = 272, Lsb18 = 274, Lsb20 = 276, Lsb24 = 280 };
/// I2S sample rate (AUDIO cmd 22), 10 Hz units.
enum class I2sRate : uint16_t { Rate32k = 3200, Rate44k1 = 4410, Rate48k = 4800 };
/// Wave generator mode (AUDIO cmd 24).
enum class WaveMode : uint8_t { Off = 0, Wave1Left = 1, Wave2Right = 2, Both = 3, Wave1Stereo = 5, Wave2Stereo = 6, Sum = 7 };

/// The four weak-signal handling functions (they share one parameter model).
enum class WshFunction : uint8_t { Softmute, Highcut, Stereo, StHiBlend };

// ============================================================================
// Read results
// ============================================================================

/// APPL Get_Identification (user manual 4.9).
struct Identification {
  uint16_t device = 0;     ///< [15:8] type (9 = TEF668X), [7:0] variant
  uint16_t hwVersion = 0;  ///< major.minor
  uint16_t swVersion = 0;  ///< major.minor (V102 = hw 1, sw 2)
};

/**
 * @brief Reception quality (FM/AM Get_Quality, user manual 4.1). One I2C read fills
 *        it; the accessors convert to readable units without floating point.
 */
struct Quality {
  Band band = Band::FM;
  uint16_t status = 0;      ///< [15] AF_Update sample, [9:0] time since tune (0 = not ready)
  int16_t level = 0;        ///< RF level, 0.1 dBuV
  uint16_t noise = 0;       ///< FM ultrasonic noise (0.1 %), AM noise (0.1 %)
  uint16_t word4 = 0;       ///< FM multipath (0.1 %), AM co-channel flag
  int16_t offset = 0;       ///< frequency offset, 0.1 kHz
  uint16_t bandwidth = 0;   ///< IF bandwidth, 0.1 kHz
  uint16_t modulation = 0;  ///< modulation, 0.1 %

  /// Data is valid (the tuner finished tuning).
  bool ready() const { return (status & 0x03FF) != 0; }
  /// Data was sampled by an AF_Update tune.
  bool afSample() const { return (status & 0x8000) != 0; }
  /// Time since the tune in 0.1 ms (1000 = settled, updated continuously).
  uint16_t age() const { return status & 0x03FF; }
  /// RF level in 0.1 dBuV.
  int16_t rssi() const { return level; }
  /// FM multipath (WAM) in 0.1 %; 0 on AM.
  uint16_t multipath() const { return band == Band::FM ? word4 : 0; }
  /// AM co-channel detected (see setCoChannelDet); false on FM.
  bool coChannel() const { return band == Band::AM && word4 != 0; }
  /// 0...100: level 10 dBuV -> 0, 50 dBuV -> 100, reduced by FM noise and multipath.
  uint8_t signalPercent() const;
};

/// FM/AM Get_AGC (user manual 4.3), 0.1 dB, 6 dB steps.
struct AgcStatus {
  uint16_t inputAttenuation = 0;
  uint16_t feedbackAttenuation = 0;
};

/// FM/AM Get_Signal_Status (user manual 4.4).
struct SignalStatus {
  bool stereo = false;   ///< FM stereo pilot detected
  bool digital = false;  ///< digital radio blended in (TEF6688/89)
};

/// FM/AM Get_Processing_Status (user manual 4.5): live weak-signal control, 0.1 %.
struct ProcessingStatus {
  uint16_t softmute = 0;
  uint16_t highcut = 0;
  uint16_t stereo = 0;         ///< 1000 = mono
  uint16_t stHiBlend = 0;
  uint8_t fmsiBand[4] = {0};   ///< FMSI bands 1..4 in % (TEF6687/89)
};

/// APPL Get_LastWrite (user manual 4.10): what the chip received last.
struct LastWrite {
  uint8_t paramCount = 0;
  uint8_t module = 0;
  uint8_t cmd = 0;
  uint8_t index = 0;
  uint16_t params[5] = {0};
};

/// FM Get_RDS (user manual 4.2), decoder mode.
struct RdsGroup {
  uint16_t status = 0;          ///< [15] available [14] lost [13] first PI [12] type B [9] synced
  uint16_t block[4] = {0};      ///< A, B, C, D
  uint16_t errors = 0;          ///< 2 bits per block A..D in [15:8]: 0 ok, 1 small, 2 large, 3 uncorrectable

  bool available() const { return (status & 0x8000) != 0; }
  bool dataLost() const { return (status & 0x4000) != 0; }
  bool firstPi() const { return (status & 0x2000) != 0; }
  bool typeB() const { return (status & 0x1000) != 0; }
  bool synced() const { return (status & 0x0200) != 0; }
  /// Error level of block i (0 = A ... 3 = D).
  uint8_t error(uint8_t i) const { return (errors >> (14 - 2 * i)) & 0x03; }
};

/**
 * @brief One weak-signal handling function (SoftMute, HighCut, Stereo or StHiBlend)
 *        as a whole. Read the chip defaults with TEF668X::wshDefaults(), change some
 *        fields, write it with TEF668X::setWsh(). See notes in TEF668X.h.
 */
struct WshConfig {
  struct Timing { uint16_t slowAttack, slowDecay, fastAttack, fastDecay; };  ///< ms, ms, 0.1 ms, 0.1 ms
  struct Detector { WshMode mode; uint16_t start, slope; };                ///< level 0.1 dBuV / noise 0.1 %
  struct Mod { bool enable; uint16_t start, slope, shift; };               ///< 0.1 %
  struct Limit { uint8_t mode; uint16_t limit; };                          ///< mode 0/1 (Stereo min: 0/1/2)

  Timing time;
  Detector level, noise, mph;
  Mod mod;
  Limit max, min;
};

/// Parts of a WshConfig to write (bit mask, combine with |).
enum WshPart : uint8_t {
  kWshTime = 0x01, kWshLevel = 0x02, kWshNoise = 0x04, kWshMph = 0x08,
  kWshMod = 0x10, kWshMax = 0x20, kWshMin = 0x40, kWshAll = 0x7F
};

}  // namespace tef
