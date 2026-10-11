#pragma once
/**
 * @file TEF668X.h
 * @brief Driver for the NXP TEF6686 / TEF6687 / TEF6688 / TEF6689 ("Lithio") radio tuners.
 *
 * ## How the library is built (read this first)
 *
 * One chip, four levels of access. Pick the level you need; they can be mixed.
 *
 * | Level | What                         | Example                                              |
 * |-------|------------------------------|------------------------------------------------------|
 * | L3    | presets                      | `radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);`         |
 * | L2    | feature helpers              | `radio.setWsh(WshFunction::Softmute, Band::FM, cfg);`|
 * | L1    | one function per command     | `radio.setSoftmuteMax(Band::FM, true, 150);`         |
 * | L0    | raw command / raw words      | `radio.write(TEFCmd::FmSoftmuteMax, p, 2);`        |
 *
 * - **Nothing is configured behind your back.** begin() + init() only bring the chip up
 *   (patch, clock, activate, GPIO wiring from BoardConfig). The chip then runs with its
 *   own defaults and is **muted**. All radio settings are your choice: call L1/L2
 *   functions or apply a preset.
 * - **Every write is checked** against the user-manual ranges, the detected variant
 *   (FMSI / digital radio), the enabled bands and the device state, using one table
 *   generated from tools/commands.py. A refused write returns an Error and sends nothing.
 * - **No heap, no String, no float, no virtual functions.** Tables and presets stay in
 *   flash (PROGMEM on AVR/ESP8266). Unused functions cost no flash.
 * - Optional parts are separate objects that cost nothing until you create them:
 *   TEF668XRds (RDS decoding), TEF668XSeek (non-blocking seek), TEF668XShadow (copy of
 *   the last written settings).
 *
 * ## Minimal sketch
 * @code
 * #include <Wire.h>
 * #include <TEF668X.h>
 * TEF668X radio;
 * void setup() {
 *   Wire.begin();
 *   radio.begin(Wire);                      // default board: 9.216 MHz, 0x64, FM+AM
 *   radio.init();                           // patch if needed, start, activate
 *   radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);
 *   radio.tuneFM(10470);                    // 104.70 MHz
 *   radio.setVolumePercent(80);             // 80 % (-10 dB)
 *   radio.setMute(false);                   // the chip starts muted
 * }
 * @endcode
 */
#include <Arduino.h>
#include <Wire.h>

#include "TEF668XCommands.h"
#include "TEF668XPresets.h"
#include "TEF668XTypes.h"

class TEF668XShadow;

class TEF668X {
 public:
  using Error = tef::Error;
  using Band = tef::Band;
  using Module = tef::Module;
  using Variant = tef::Variant;
  using OpState = tef::OpState;
  using WshMode = tef::WshMode;
  using WshFunction = tef::WshFunction;
  using WshConfig = tef::WshConfig;
  using TuneMode = tef::TuneMode;
  using Restart = tef::Restart;
  using Deemphasis = tef::Deemphasis;
  using HighcutFilter = tef::HighcutFilter;
  using StereoMin = tef::StereoMin;
  using RdsMode = tef::RdsMode;
  using RdsPin = tef::RdsPin;
  using QsiPin = tef::QsiPin;
  using DrBlend = tef::DrBlend;
  using AnaOutSpecial = tef::AnaOutSpecial;
  using AudioSource = tef::AudioSource;
  using AudioOutput = tef::AudioOutput;
  using I2sSignal = tef::I2sSignal;
  using I2sMode = tef::I2sMode;
  using I2sFormat = tef::I2sFormat;
  using I2sRate = tef::I2sRate;
  using WaveMode = tef::WaveMode;
  using GpioFeature = tef::GpioFeature;

  /// How init() brings the chip up.
  enum class InitMode : uint8_t {
    Auto,     ///< read the state: boot -> upload the patch, idle -> activate, active -> keep running
    Patch,    ///< always (re)load the patch; uses the undocumented "14 00 00" reset if not in boot
    NoPatch   ///< never upload; refuses a chip in boot state (it would run without the required patch)
  };

  // ==========================================================================
  // Setup and health
  // ==========================================================================

  /**
   * @brief Attach the I2C bus and describe the board. Sends nothing except the
   *        tuner power switching when BoardConfig::power == TunerPower::EnablePin.
   * @param wire  an already started TwoWire (call Wire.begin() first)
   * @param board crystal, address, bands, power and GPIO wiring (see TEFBoard)
   * @return Error::Ok, or Error::InvalidArg for an unsupported clock / address / pin.
   * @details With TunerPower::Switched or EnablePin it waits powerUpDelayMs because the
   *          TEF must not see I2C traffic during its first 5 ms (AN11254 2.8).
   */
  Error begin(TwoWire& wire, const tef::BoardConfig& board = tef::BoardConfig());

  /**
   * @brief Bring the chip to the active state (radio standby, muted).
   * @param mode      see InitMode; Auto is right for almost every case
   * @param timeoutMs maximum wait per state change
   * @details Sequence (user manual 6.2): [boot] patch + LUT -> Start -> [idle]
   *          reference clock -> Activate -> [standby] -> identify the variant ->
   *          program the TEF GPIOs from BoardConfig::gpio. No radio setting is written.
   */
  Error init(InitMode mode = InitMode::Auto, uint16_t timeoutMs = 1200);

  /// True after a successful init() and as long as checkAlive() did not find a reset.
  bool isReady() const { return ready_; }

  /**
   * @brief Detect a power dip / clock loss: the chip then falls back to boot state and
   *        loses every setting (user manual 6.3.1).
   * @return Error::Ok if still running, Error::BadState if it was reset (isReady() becomes
   *         false: call init() and re-apply your settings), Error::I2CError if absent.
   */
  Error checkAlive();

  /// The board description given to begin().
  const tef::BoardConfig& board() const { return board_; }

  /// Variant read at init (Variant::Unknown before).
  Variant variant() const { return variant_; }
  /// Identification words read at init.
  const tef::Identification& identification() const { return ident_; }
  /// TEF6687 / TEF6689: FMSI stereo improvement available.
  bool supportsFmsi() const { return variant_ == Variant::TEF6687 || variant_ == Variant::TEF6689; }
  /// TEF6688 / TEF6689: digital radio baseband output available.
  bool supportsDigitalRadio() const { return variant_ == Variant::TEF6688 || variant_ == Variant::TEF6689; }

  // ==========================================================================
  // L0 - raw access
  // ==========================================================================

  /**
   * @brief Send one command through the checked path (all L1/L2/L3 calls end here).
   * @param cmd    command id (TEFCmd::FmSoftmuteMax ...)
   * @param params parameter words, signed or unsigned as in the user manual
   * @param count  between the required and the maximum parameter count of cmd
   * @details Checks: init done, band enabled, variant, state, count, ranges. Also
   *          enforces the AM->FM AGC timing rule (user manual 3.4). On success the
   *          attached TEF668XShadow (if any) records the values.
   * @return Error::Ok or the reason; for Error::OutOfRange see lastErrorParam().
   */
  Error write(tef::Cmd cmd, const int16_t* params, uint8_t count);

  /// Index (0-based) of the parameter that caused the last Error::OutOfRange.
  uint8_t lastErrorParam() const { return lastErrorParam_; }

  /**
   * @brief Unchecked write of any module/command (experiments, future commands).
   * @details Frame: module, cmd, index 1, then the words MSB first (user manual 5.2).
   */
  Error writeWords(Module module, uint8_t cmd, const uint16_t* words, uint8_t count);

  /**
   * @brief Read words of any Get command.
   * @param index 1 = normal; 0 = first word is the confirmation / error code (5.7.2)
   * @details Write request, STOP, 30 us pause, read: meets the 50 us data setup of
   *          user manual 5.6 at any bus speed. Reading fewer words than defined is allowed.
   */
  Error readWords(Module module, uint8_t cmd, uint16_t* out, uint8_t count, uint8_t index = 1);

  // ==========================================================================
  // L1 - one typed function per user-manual command (generated, documented)
  // ==========================================================================
#include "TEF668XCommandMethods.inc"

  /**
   * @brief Program a TEF GPIO by hand (normally done at init from BoardConfig::gpio).
   * @param pin     0, 1 or 2 (GPIO_1/2 are outputs only)
   * @param band    which radio mode this function applies to (FM and AM keep separate settings)
   * @param feature function (all signals are active low)
   */
  Error setGpio(uint8_t pin, Band band, GpioFeature feature);

  /**
   * @brief Configure the I2S input or output (AUDIO cmd 22, user manual 3.35).
   * @param master true = the TEF drives BCK/WS; false = slave (advised with a DR decoder)
   */
  Error setDigIo(I2sSignal signal, I2sMode mode, I2sFormat format, bool master, I2sRate rate);

  // ==========================================================================
  // Tuning
  // ==========================================================================

  /**
   * @brief Tune FM. Also switches from AM to FM.
   * @param frequency 6500 ... 10800 in 10 kHz (10470 = 104.70 MHz)
   * @param mode      Preset (normal), Search (stay muted), AfUpdate / Jump / Check (RDS AF)
   * @details Returns immediately; quality becomes valid a few ms later (Quality::ready()).
   *          Preset mutes ~32 ms; a band change adds ~15 ms (user manual 3.1.1).
   */
  Error tuneFM(uint16_t frequency, TuneMode mode = TuneMode::Preset);

  /**
   * @brief Tune AM (LW 144-288, MW 522-1710, SW 2300-27000 kHz). Also switches from FM.
   * @param mode Preset or Search only (AF modes are FM only).
   * @details If a GPIO is wired as AgcStep it is driven high in AM (AN11254 2.1).
   */
  Error tuneAM(uint16_t frequencyKHz, TuneMode mode = TuneMode::Preset);

  /// Release the mute left by a Search or Check tune (Tune_To mode 7).
  Error tuneEnd();

  /// Band of the last tune (FM after init).
  Band band() const { return band_; }
  /// Frequency of the last tune (10 kHz on FM, kHz on AM), 0 if none.
  uint16_t frequency() const { return frequency_; }

  // ==========================================================================
  // Reads (results in tef:: structures with unit accessors)
  // ==========================================================================

  /**
   * @brief Reception quality of the current band.
   * @param release true = Get_Quality_Data (releases AF_Update data for new
   *                measurements), false = Get_Quality_Status (keeps it frozen)
   */
  Error readQuality(tef::Quality& out, bool release = true);
  /// RF AGC attenuation of the current band.
  Error readAgc(tef::AgcStatus& out);
  /// Stereo pilot / digital blend of the current band.
  Error readSignal(tef::SignalStatus& out);
  /// Live weak-signal processing (how much SoftMute/HighCut/blend is applied now).
  Error readProcessing(tef::ProcessingStatus& out);
  /// Digital radio baseband sample rate (0 / 6500 / 6750), TEF6688/89 only.
  Error readDrSampleRate(uint16_t& out);
  /// Device state (works in boot and idle too).
  Error readOperationState(OpState& out);
  /// Level of GPIO_0 when it is set to GpioFeature::Input.
  Error readGpio0(bool& high);
  /// Identification words (also stored by init()).
  Error readIdentification(tef::Identification& out);
  /// What the chip received last: proves a command arrived as intended.
  Error readLastWrite(tef::LastWrite& out);
  /**
   * @brief One RDS group from the chip buffer (22 groups deep).
   * @param release true = Get_RDS_Data (pops the group), false = Get_RDS_Status (peek)
   * @details Read at least every 87 ms, or empty the buffer in a burst. TEF668XRds
   *          does this and decodes the services for you.
   */
  Error readRds(tef::RdsGroup& out, bool release = true);

  // ==========================================================================
  // L2 - feature helpers
  // ==========================================================================

  /**
   * @brief Write one weak-signal handling function in one call.
   * @param fn    Softmute, Highcut (incl. LowCut limits), Stereo or StHiBlend
   * @param band  Band::AM only for Softmute and Highcut
   * @param cfg   all settings of that function (start from wshDefaults())
   * @param parts which sub-commands to send (TEF_WSH_PART_TIME | TEF_WSH_PART_LEVEL ...);
   *              parts that do not exist for fn/band are skipped
   * @details Sends _Time, _Level, _Noise, _Mph, _Mod, _Max, _Min as selected. Highcut
   *          `min` maps to Highcut_Min; LowCut stays on setLowcutMax/Min.
   */
  Error setWsh(WshFunction fn, Band band, const WshConfig& cfg, uint8_t parts = tef::kWshAll);

  /// Fill cfg with the chip defaults of a weak-signal function (from the command table).
  Error wshDefaults(WshFunction fn, Band band, WshConfig& cfg) const;

  /**
   * @brief Write the documented chip default of every setting of one band (FM or AM).
   * @details Gives a known baseline after experiments. Skips features the variant
   *          does not have. Volume, mute, audio routing and GPIOs are not touched.
   */
  Error restoreChipDefaults(Band band);

  /**
   * @brief Chip default parameters of one command.
   * @param out   receives up to 7 words
   * @param count receives the number of parameters
   * @return Error::InvalidArg if the command has no documented defaults.
   */
  Error chipDefaults(tef::Cmd cmd, int16_t* out, uint8_t& count) const;

  /**
   * @brief Volume as a percentage, like a volume knob.
   * @param percent 0 = silent, 1...100 = -49.5 dB ... 0 dB in 0.5 dB steps (100 = no gain)
   * @details Equal steps in dB sound even to the ear. For a boost above 0 dB (up to
   *          +24 dB) use setVolume() in 0.1 dB. The value is remembered in volumePercent().
   */
  Error setVolumePercent(uint8_t percent);
  /// Last value given to setVolumePercent() (0 before the first call).
  uint8_t volumePercent() const { return volumePercent_; }

  /// Write the de-emphasis of a region (EU/JP/OIRT 50 us, USA 75 us).
  Error applyRegion(tef::Region region);

  // ==========================================================================
  // L3 - presets
  // ==========================================================================

  /**
   * @brief Apply a preset: a list of commands stored in flash (see TEF668XPresets.h).
   * @details Each entry goes through write(). Stops at the first error; the failing
   *          entry number is in lastPresetEntry(). User presets use the same format.
   */
  Error applyPreset(const uint8_t* preset);
  /// Entry number (0-based) where the last applyPreset() stopped.
  uint8_t lastPresetEntry() const { return lastPresetEntry_; }

  /// Attach a TEF668XShadow that records every successful write (nullptr to detach).
  void attachShadow(TEF668XShadow* shadow) { shadow_ = shadow; }

 private:
  Error requireBegun() const;
  Error rawWrite(const uint8_t* data, uint8_t len);
  Error sendWords(uint8_t module, uint8_t cmd, const int16_t* words, uint8_t count);
  Error waitState(OpState target, uint16_t timeoutMs);
  Error uploadPatch();
  Error writePatchStream(const unsigned char* bytes, size_t size);
  Error activateFromIdle(uint16_t timeoutMs);
  Error identify();
  Error applyGpioWiring();
  Error tune(Band band, uint16_t frequency, TuneMode mode);
  bool bandEnabled(Band band) const { return band == Band::FM ? board_.fm : board_.am; }
  Module currentModule() const { return band_ == Band::FM ? Module::FM : Module::AM; }

  TwoWire* wire_ = nullptr;
  TEF668XShadow* shadow_ = nullptr;
  tef::BoardConfig board_;
  tef::Identification ident_;
  Variant variant_ = Variant::Unknown;
  Band band_ = Band::FM;
  uint16_t frequency_ = 0;
  uint32_t fmAgcWriteMs_ = 0;    // AM->FM AGC timing rule
  uint32_t bandChangeMs_ = 0;
  bool begun_ = false;
  bool ready_ = false;
  uint8_t lastErrorParam_ = 0;
  uint8_t lastPresetEntry_ = 0;
  uint8_t volumePercent_ = 0;
};

#include "TEF668XRds.h"
#include "TEF668XSeek.h"
#include "TEF668XShadow.h"
#include "TEF668XNames.h"
