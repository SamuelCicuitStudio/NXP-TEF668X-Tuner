#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * @file TEF668X.h
 * @brief Public API for TEF668X Arduino/ESP library.
 */

/**
 * @class TEF668X
 * @brief Control class for NXP TEF668X family radios over I2C.
 *
 * Supports TEF6686/6687/6688/6689 with:
 * - startup state machine (soft/hard init)
 * - patch/LUT upload
 * - FM/AM tuning
 * - audio controls
 * - variant-gated FMSI / Digital Radio commands
 *
 * Typical startup flow:
 * @code{.cpp}
 * TEF668X radio;
 * radio.begin(Wire, 0x64);
 * auto e = radio.initAuto();   // or initHard()
 * if (e == TEF668X::Error::Ok) {
 *   radio.applyFmDefaultProfile();
 *   radio.tuneFmPreset(10170); // 101.7 MHz
 * }
 * @endcode
 */
class TEF668X {
public:
  /**
   * @brief Generic library error codes.
   */
  enum class Error : uint8_t {
    Ok = 0,             /**< Operation completed successfully. */
    NotBegun,           /**< `begin()` not called yet. */
    I2CError,           /**< I2C transaction failed. */
    Timeout,            /**< Poll/wait operation exceeded timeout. */
    InvalidArg,         /**< One or more parameters are invalid. */
    UnsupportedFeature, /**< API not supported on active variant. */
    BadState,           /**< Device state does not allow this operation. */
    PatchDataError      /**< Patch/LUT data is missing or malformed. */
  };

  /**
   * @brief TEF668X variant identifiers (low byte from APPL Get_Identification).
   */
  enum class Variant : uint8_t {
    Unknown = 0xFF, /**< Unknown or not yet detected. */
    TEF6686 = 14,   /**< TEF6686 variant id. */
    TEF6687 = 1,    /**< TEF6687 variant id (FMSI-capable). */
    TEF6688 = 9,    /**< TEF6688 variant id (DR-capable). */
    TEF6689 = 3     /**< TEF6689 variant id (FMSI + DR capable). */
  };

  /**
   * @brief Variant selection policy used by the library.
   */
  enum class VariantPolicy : uint8_t {
    /** Automatically use the detected hardware variant. */
    AutoDetect = 0,
    /** Force TEF6686 feature map even if detection differs. */
    ForceTEF6686,
    /** Force TEF6687 feature map even if detection differs. */
    ForceTEF6687,
    /** Force TEF6688 feature map even if detection differs. */
    ForceTEF6688,
    /** Force TEF6689 feature map even if detection differs. */
    ForceTEF6689
  };

  /**
   * @brief Operation states returned by APPL cmd 128.
   */
  enum class OpState : uint16_t {
    Boot = 0,         /**< Bootloader/patch mode. */
    Idle = 1,         /**< Application idle state. */
    ActiveStandby = 2,/**< Active core, no FM/AM path selected. */
    ActiveFm = 3,     /**< FM signal path active. */
    ActiveAm = 4      /**< AM signal path active. */
  };

  /**
   * @brief Device identity tuple from APPL Get_Identification.
   */
  struct Identification {
    /** Raw device word: [15:8]=type, [7:0]=variant id. */
    uint16_t device = 0;
    /** Hardware version word. */
    uint16_t hwVersion = 0;
    /** Firmware version word. */
    uint16_t swVersion = 0;
  };

  /**
   * @brief FM quality data from FM cmd 129 Get_Quality_Data.
   */
  struct QualityData {
    /** Status bitfield from firmware. */
    uint16_t status = 0;
    /** RF level in 0.1 dBuV. */
    int16_t level_x10_dBuV = 0;
    /** USN metric in 0.1 %. */
    uint16_t usn_x10_pct = 0;
    /** WAM (multipath) metric in 0.1 %. */
    uint16_t wam_x10_pct = 0;
    /** Frequency offset in 0.1 kHz. */
    int16_t offset_x10_kHz = 0;
    /** IF bandwidth in 0.1 kHz. */
    uint16_t bandwidth_x10_kHz = 0;
    /** Modulation in 0.1 %. */
    uint16_t modulation_x10_pct = 0;
  };

  /**
   * @brief FM weak-signal processing status from FM cmd 134.
   */
  struct ProcessingStatusFm {
    /** Softmute state in 0.1 %. */
    uint16_t softmute_x10_pct = 0;
    /** Highcut state in 0.1 %. */
    uint16_t highcut_x10_pct = 0;
    /** Stereo blend state in 0.1 %. */
    uint16_t stereo_x10_pct = 0;
    /** Stereo high-blend state in 0.1 %. */
    uint16_t stHiBlend_x10_pct = 0;
  };

  /**
   * @brief Construct a TEF668X controller object.
   *
   * @note Hardware communication starts only after calling `begin()`.
   */
  TEF668X() = default;

  /**
   * @brief Attach an I2C bus and device address.
   * @param wire I2C bus object (`Wire` or other `TwoWire` instance).
   * @param i2cAddress 7-bit I2C address (default TEF address is 0x64).
   * @return `true` on success.
   *
   * @note Must be called before any API that talks to hardware.
   */
  bool begin(TwoWire& wire = Wire, uint8_t i2cAddress = 0x64);

  /**
   * @brief Set I2C clock rates used by patch stage and runtime stage.
   * @param patchHz I2C clock used during hard init patch upload.
   * @param runHz I2C clock used during normal operation.
   */
  void setI2CSpeeds(uint32_t patchHz, uint32_t runHz);

  /**
   * @brief Set patch upload chunk size in bytes.
   * @param chunkBytes Chunk length; library enforces even byte count.
   *
   * @note TEF patch stream requires 2-byte boundary splitting.
   */
  void setPatchChunkSize(size_t chunkBytes);

  /**
   * @brief Configure variant policy (`AutoDetect` or forced variant).
   * @param policy Variant policy to apply after identification.
   */
  void setVariantPolicy(VariantPolicy policy);

  /**
   * @brief Read current variant policy.
   * @return Current `VariantPolicy`.
   */
  VariantPolicy getVariantPolicy() const;

  /**
   * @brief Soft initialization (no patch upload).
   * @param crystalHz Reference clock frequency in Hz.
   * @param externalClock `true` for external reference, `false` for crystal.
   * @param timeoutMs State transition timeout in milliseconds.
   * @return Initialization status.
   * @retval Error::Ok Init sequence completed.
   * @retval Error::NotBegun `begin()` was not called.
   * @retval Error::I2CError Device did not acknowledge an I2C command.
   * @retval Error::Timeout State transition did not complete in time.
   *
   * @details Sequence: Stop -> Start -> wait Idle -> Set_ReferenceClock -> Activate -> wait ActiveStandby -> Identify.
   */
  Error initSoft(uint32_t crystalHz = 9216000UL, bool externalClock = false, uint32_t timeoutMs = 800);

  /**
   * @brief Hard initialization with official patch/LUT upload.
   * @param crystalHz Reference clock frequency in Hz.
   * @param externalClock `true` for external reference, `false` for crystal.
   * @param timeoutMs State transition timeout in milliseconds.
   * @return Initialization status.
   * @retval Error::Ok Init, patch, and activation completed.
   * @retval Error::PatchDataError Embedded patch/LUT data invalid.
   * @retval Error::NotBegun `begin()` was not called.
   * @retval Error::I2CError Device communication failed.
   * @retval Error::Timeout State transition timed out.
   */
  Error initHard(uint32_t crystalHz = 9216000UL, bool externalClock = false, uint32_t timeoutMs = 1200);

  /**
   * @brief Try soft init first, then fallback to hard init.
   * @param crystalHz Reference clock frequency in Hz.
   * @param externalClock `true` for external reference, `false` for crystal.
   * @return Initialization status from soft or hard path.
   */
  Error initAuto(uint32_t crystalHz = 9216000UL, bool externalClock = false);

  /**
   * @brief Apply default FM profile alias.
   * @param volume_x10_dB Output volume in 0.1 dB (signed).
   * @return Profile apply status.
   *
   * @note Currently maps to `applyFmCarProfile`.
   */
  Error applyFmDefaultProfile(int16_t volume_x10_dB = -80);

  /**
   * @brief Apply TEF6686-oriented FM "car" profile.
   * @param volume_x10_dB Output volume in 0.1 dB (signed).
   * @return Profile apply status.
   * @retval Error::Ok All profile writes completed.
   * @retval Error::NotBegun `begin()` was not called.
   * @retval Error::I2CError At least one write failed.
   *
   * @details Writes a full FM setup set (bandwidth, AGC, weak-signal controls, stereo controls, RDS, scaler, volume).
   */
  Error applyFmCarProfile(int16_t volume_x10_dB = -80);

  /**
   * @brief Tune FM using Preset action (`mode=1`).
   * @param frequency_kHz FM frequency in TEF units (`6500..10800`, 10 kHz step).
   * @return Tune command status.
   * @retval Error::Ok Tune command accepted.
   * @retval Error::InvalidArg Frequency outside valid FM range.
   * @retval Error::NotBegun `begin()` was not called.
   * @retval Error::I2CError I2C write failed.
   */
  Error tuneFmPreset(uint16_t frequency_kHz);

  /**
   * @brief Tune FM using Search action (`mode=2`), stays muted until release.
   * @param frequency_kHz FM frequency in TEF units (`6500..10800`, 10 kHz step).
   * @return Tune command status (same codes as `tuneFmPreset`).
   */
  Error tuneFmSearch(uint16_t frequency_kHz);

  /**
   * @brief Tune AM using Preset action (`mode=1`).
   * @param frequency_kHz AM frequency in kHz (`LW/MW/SW` supported ranges).
   * @return Tune command status.
   * @retval Error::Ok Tune command accepted.
   * @retval Error::InvalidArg Frequency outside supported AM ranges.
   * @retval Error::NotBegun `begin()` was not called.
   * @retval Error::I2CError I2C write failed.
   */
  Error tuneAmPreset(uint16_t frequency_kHz);

  /**
   * @brief Release mute after Search/Check action (`mode=7`).
   * @return Command status.
   * @retval Error::Ok Release command sent.
   * @retval Error::BadState Device is not in FM or AM active state.
   *
   * @details Uses current operation state to choose FM or AM module.
   */
  Error releaseSearchOrCheckMuteFmAm();

  /**
   * @brief Set audio output volume.
   * @param volume_x10_dB Signed 0.1 dB value (e.g. `-80` => `-8.0 dB`).
   * @return Command status.
   */
  Error setVolumeDb10(int16_t volume_x10_dB);

  /**
   * @brief Enable or disable audio mute.
   * @param mute `true` to mute, `false` to unmute.
   * @return Command status.
   */
  Error setMute(bool mute);

  /**
   * @brief Program reference clock (APPL cmd 4).
   * @param frequencyHz Reference clock frequency in Hz.
   * @param externalClock `true` for external clock, `false` for crystal.
   * @return Command status.
   *
   * @note Should be called in Idle state before Activate.
   */
  Error setReferenceClock(uint32_t frequencyHz, bool externalClock);

  /**
   * @brief Read operation state (APPL cmd 128).
   * @param status Output operation state raw value.
   * @return Read status (`Error::Ok` when `status` is valid).
   */
  Error getOperationStatus(uint16_t& status);

  /**
   * @brief Read chip identification (APPL cmd 130).
   * @param out Output structure with device/hw/sw words.
   * @return Read status (`Error::Ok` when `out` is valid).
   */
  Error getIdentification(Identification& out);

  /**
   * @brief Read FM quality dataset (FM cmd 129).
   * @param out Output quality structure.
   * @return Read status (`Error::Ok` when `out` is valid).
   */
  Error getQualityFm(QualityData& out);

  /**
   * @brief Read FM signal status bitfield (FM cmd 133).
   * @param status Output status word.
   * @return Read status (`Error::Ok` when `status` is valid).
   */
  Error getSignalStatusFm(uint16_t& status);

  /**
   * @brief Read FM processing status percentages (FM cmd 134).
   * @param out Output processing status structure.
   * @return Read status (`Error::Ok` when `out` is valid).
   */
  Error getProcessingStatusFm(ProcessingStatusFm& out);

  /**
   * @brief Read Digital Radio interface status (FM/AM cmd 135).
   * @param sampleRate_kHz_x1 Output sample rate/status word.
   * @return Read status.
   * @retval Error::Ok Read succeeded.
   * @retval Error::UnsupportedFeature Active variant has no Digital Radio interface.
   */
  Error getInterfaceStatus(uint16_t& sampleRate_kHz_x1);

  /**
   * @brief Enable/disable FMSI stereo improvement (FM cmd 32).
   * @param mode FMSI mode value from manual.
   * @return Command status.
   * @retval Error::Ok Command accepted.
   * @retval Error::UnsupportedFeature Active variant has no FMSI support.
   */
  Error setStereoImprovement(uint16_t mode);

  /**
   * @brief Set FMSI band-blend timing (FM cmd 90).
   * @param attack_ms Attack in ms.
   * @param decay_ms Decay in ms.
   * @return Command status (`Error::UnsupportedFeature` on non-FMSI variants).
   */
  Error setStBandBlendTime(uint16_t attack_ms, uint16_t decay_ms);

  /**
   * @brief Set FMSI band gains (FM cmd 91).
   * @param band1 Band 1 gain/control.
   * @param band2 Band 2 gain/control.
   * @param band3 Band 3 gain/control.
   * @param band4 Band 4 gain/control.
   * @return Command status (`Error::UnsupportedFeature` on non-FMSI variants).
   */
  Error setStBandBlendGain(uint16_t band1, uint16_t band2, uint16_t band3, uint16_t band4);

  /**
   * @brief Set FMSI band bias values (FM cmd 92).
   * @param band1 Band 1 signed bias.
   * @param band2 Band 2 signed bias.
   * @param band3 Band 3 signed bias.
   * @param band4 Band 4 signed bias.
   * @return Command status (`Error::UnsupportedFeature` on non-FMSI variants).
   */
  Error setStBandBlendBias(int16_t band1, int16_t band2, int16_t band3, int16_t band4);

  /**
   * @brief Enable/disable digital radio path (FM/AM cmd 30).
   * @param mode Digital radio mode value.
   * @return Command status (`Error::UnsupportedFeature` on non-DR variants).
   */
  Error setDigitalRadio(uint16_t mode);

  /**
   * @brief Configure digital radio blend behavior (cmd 83).
   * @param mode Blend mode.
   * @param inTime_ms Analog->digital blend time in ms.
   * @param outTime_ms Digital->analog blend time in ms.
   * @param gain_x10_dB Digital path gain in 0.1 dB.
   * @return Command status (`Error::UnsupportedFeature` on non-DR variants).
   */
  Error setDrBlend(uint16_t mode, uint16_t inTime_ms, uint16_t outTime_ms, int16_t gain_x10_dB);

  /**
   * @brief Configure digital radio interface options (cmd 84).
   * @param sampleRate Sample rate field.
   * @param mode DR interface mode.
   * @param format DR data format.
   * @return Command status (`Error::UnsupportedFeature` on non-DR variants).
   */
  Error setDrOptions(uint16_t sampleRate, uint16_t mode, uint16_t format);

  /**
   * @brief Active variant used by feature checks (detected or forced).
   * @return Active variant.
   *
   * @note If policy is forced, this may differ from `getDetectedVariant()`.
   */
  Variant getActiveVariant() const;

  /**
   * @brief Variant detected from hardware identification.
   * @return Detected variant, or `Variant::Unknown` if not detected.
   */
  Variant getDetectedVariant() const;

  /**
   * @brief `true` if forced variant does not match detected variant.
   * @return Mismatch flag.
   */
  bool hasVariantMismatch() const;

  /**
   * @brief Check if active variant supports FMSI APIs.
   * @return `true` for TEF6687/TEF6689.
   */
  bool supportsFmsi() const;

  /**
   * @brief Check if active variant supports Digital Radio APIs.
   * @return `true` for TEF6688/TEF6689.
   */
  bool supportsDigitalRadio() const;

  /**
   * @brief Whether library passed initialization and is ready for normal use.
   * @return Ready state flag.
   */
  bool isReady() const;

  /**
   * @brief Low-level unsigned word write helper.
   * @param module TEF module id.
   * @param cmd Command id.
   * @param index Command index byte.
   * @param words Pointer to payload words.
   * @param wordCount Number of payload words.
   * @return Command status.
   *
   * @warning Advanced API. Prefer high-level helpers unless implementing raw TEF commands.
   */
  Error writeWordsU16(uint8_t module, uint8_t cmd, uint8_t index, const uint16_t* words, uint8_t wordCount);

  /**
   * @brief Low-level signed word write helper.
   * @param module TEF module id.
   * @param cmd Command id.
   * @param index Command index byte.
   * @param words Pointer to payload words.
   * @param wordCount Number of payload words.
   * @return Command status.
   *
   * @warning Advanced API. Prefer high-level helpers unless implementing raw TEF commands.
   */
  Error writeWordsS16(uint8_t module, uint8_t cmd, uint8_t index, const int16_t* words, uint8_t wordCount);

  /**
   * @brief Low-level word read helper.
   * @param module TEF module id.
   * @param cmd Command id.
   * @param index Command index byte.
   * @param out Output buffer for words.
   * @param wordCount Number of words to read.
   * @return Read status.
   *
   * @warning Advanced API. Prefer high-level helpers unless implementing raw TEF commands.
   */
  Error readWords(uint8_t module, uint8_t cmd, uint8_t index, uint16_t* out, uint8_t wordCount);

private:
  Error writeRaw(const uint8_t* data, size_t len);
  Error stopBoot();
  Error startBoot();
  Error activate();
  Error waitForState(uint16_t target, uint32_t timeoutMs);
  Error uploadPatchAndLut();
  Error writePatchChunk(const unsigned char* bytes, size_t offset, size_t len);
  Error detectVariant();
  static Variant policyToVariant(VariantPolicy policy);
  Error requireFmsiFeature() const;
  Error requireDigitalRadioFeature() const;

  static bool isFmFrequencyValid(uint16_t frequency_kHz);
  static bool isAmFrequencyValid(uint16_t frequency_kHz);

private:
  TwoWire* wire_ = nullptr;
  uint8_t addr_ = 0x64;
  uint32_t i2cPatchHz_ = 100000UL;
  uint32_t i2cRunHz_ = 100000UL;
  size_t patchChunkBytes_ = 24;
  bool begun_ = false;
  bool ready_ = false;
  Identification identification_{};
  VariantPolicy variantPolicy_ = VariantPolicy::AutoDetect;
  Variant activeVariant_ = Variant::Unknown;
  Variant detectedVariant_ = Variant::Unknown;
  bool variantMismatch_ = false;
};
