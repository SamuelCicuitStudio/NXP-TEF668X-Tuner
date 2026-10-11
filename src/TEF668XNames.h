#pragma once
/**
 * @file TEF668XNames.h
 * @brief Uppercase names for the values used most, for easy-to-read sketches.
 *
 * Each name is just another spelling of a typed value from TEF668XTypes.h /
 * TEF668XPresets.h, so the compiler still checks it (passing TEF_BAND_FM where a
 * preset is expected is an error). Both styles can be mixed:
 *
 * @code
 * radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);   // same as tef::kPresetNxpFieldFM
 * radio.applyRegion(TEF_REGION_EUROPE);         // same as tef::Region::Europe
 * radio.setSoftmuteLevel(TEF_BAND_FM, TEF_WSH_DUAL, 150, 220);
 * if (radio.init() != TEF_OK) { ... }
 * @endcode
 *
 * Included automatically by TEF668X.h.
 */

// ---- Presets (TEF668XPresets.h)
#define TEF_PRESET_NXP_FIELD_FM tef::kPresetNxpFieldFM          ///< NXP recommended FM values
#define TEF_PRESET_NXP_FIELD_AM_MW_SW tef::kPresetNxpFieldAmMwSw  ///< NXP recommended AM MW/SW values
#define TEF_PRESET_NXP_FIELD_AM_LW tef::kPresetNxpFieldAmLw     ///< NXP recommended AM LW values
#define TEF_PRESET_FM_CAR tef::kPresetFmCar                     ///< complete FM car setup
#define TEF_PRESET_FM_DX tef::kPresetFmDx                       ///< FM long-distance listening
#define TEF_PRESET_AM_DX tef::kPresetAmDx                       ///< AM long-distance listening
#define TEF_PRESET_AM_ACTIVE_ANTENNA tef::kPresetAmActiveAntenna ///< AM with an active antenna

// ---- Results
#define TEF_OK tef::Error::Ok
#define TEF_ERR_NOT_BEGUN tef::Error::NotBegun
#define TEF_ERR_NOT_READY tef::Error::NotReady
#define TEF_ERR_I2C tef::Error::I2CError
#define TEF_ERR_TIMEOUT tef::Error::Timeout
#define TEF_ERR_INVALID_ARG tef::Error::InvalidArg
#define TEF_ERR_OUT_OF_RANGE tef::Error::OutOfRange
#define TEF_ERR_PARAM_COUNT tef::Error::ParamCount
#define TEF_ERR_UNSUPPORTED tef::Error::UnsupportedFeature
#define TEF_ERR_BAND_DISABLED tef::Error::BandDisabled
#define TEF_ERR_BAD_STATE tef::Error::BadState
#define TEF_ERR_PATCH_DATA tef::Error::PatchDataError

// ---- Bands and regions
#define TEF_BAND_FM tef::Band::FM
#define TEF_BAND_AM tef::Band::AM
#define TEF_REGION_EUROPE tef::Region::Europe   ///< 87.5-108 MHz / 100 kHz, 50 us
#define TEF_REGION_USA tef::Region::USA         ///< 87.5-107.9 MHz / 200 kHz, 75 us
#define TEF_REGION_JAPAN tef::Region::Japan     ///< 76-95 MHz / 100 kHz, 50 us
#define TEF_REGION_OIRT tef::Region::Oirt       ///< 65.9-74 MHz / 30 kHz, 50 us

// ---- Board description (BoardConfig)
#define TEF_CLOCK_4MHZ tef::kClock4MHz
#define TEF_CLOCK_9216KHZ tef::kClock9216kHz
#define TEF_CLOCK_12MHZ tef::kClock12MHz
#define TEF_CLOCK_55466KHZ tef::kClock55466kHz   ///< external clock only
#define TEF_ADDRESS_LOW tef::kI2cAddressLow      ///< 0x64, GPIO_2 low
#define TEF_ADDRESS_HIGH tef::kI2cAddressHigh    ///< 0x65, GPIO_2 high
#define TEF_POWER_SHARED tef::TunerPower::SharedWithMcu
#define TEF_POWER_SWITCHED tef::TunerPower::Switched
#define TEF_POWER_ENABLE_PIN tef::TunerPower::EnablePin
#define TEF_GPIO_UNUSED tef::GpioFeature::Unused
#define TEF_GPIO_INPUT tef::GpioFeature::Input
#define TEF_GPIO_LOW tef::GpioFeature::Low
#define TEF_GPIO_HIGH tef::GpioFeature::High
#define TEF_GPIO_RDS_DAVN tef::GpioFeature::Davn
#define TEF_GPIO_QSI tef::GpioFeature::Qsi
#define TEF_GPIO_QSI_DAVN tef::GpioFeature::QsiDavn
#define TEF_GPIO_AGC_STEP tef::GpioFeature::AgcStep

// ---- Tuning
#define TEF_TUNE_PRESET tef::TuneMode::Preset
#define TEF_TUNE_SEARCH tef::TuneMode::Search
#define TEF_TUNE_AF_UPDATE tef::TuneMode::AfUpdate
#define TEF_TUNE_JUMP tef::TuneMode::Jump
#define TEF_TUNE_CHECK tef::TuneMode::Check
#define TEF_DEEMPHASIS_OFF tef::Deemphasis::Off
#define TEF_DEEMPHASIS_50US tef::Deemphasis::Us50
#define TEF_DEEMPHASIS_75US tef::Deemphasis::Us75

// ---- Weak-signal handling
#define TEF_WSH_OFF tef::WshMode::Off
#define TEF_WSH_FAST tef::WshMode::Fast
#define TEF_WSH_SLOW tef::WshMode::Slow
#define TEF_WSH_DUAL tef::WshMode::Dual
#define TEF_SOFTMUTE tef::WshFunction::Softmute
#define TEF_HIGHCUT tef::WshFunction::Highcut
#define TEF_STEREO tef::WshFunction::Stereo
#define TEF_STHIBLEND tef::WshFunction::StHiBlend
#define TEF_HIGHCUT_IIR tef::HighcutFilter::Iir
#define TEF_HIGHCUT_DEEMPHASIS tef::HighcutFilter::Deemphasis
#define TEF_HIGHCUT_FIR tef::HighcutFilter::Fir
#define TEF_STEREO_MIN_OFF tef::StereoMin::Off
#define TEF_STEREO_MIN_LIMIT tef::StereoMin::Limit
#define TEF_STEREO_FORCED_MONO tef::StereoMin::ForcedMono

// ---- RDS
#define TEF_RDS_OFF tef::RdsMode::Off
#define TEF_RDS_DECODER tef::RdsMode::Decoder
#define TEF_RDS_DEMODULATOR tef::RdsMode::Demodulator
#define TEF_RDS_STRICT tef::RdsPolicy::Strict
#define TEF_RDS_NORMAL tef::RdsPolicy::Normal
#define TEF_RDS_FAST tef::RdsPolicy::Fast
#define TEF_RDS_DX tef::RdsPolicy::Dx

// ---- Audio
#define TEF_SOURCE_RADIO tef::AudioSource::Radio
#define TEF_SOURCE_I2S_IN tef::AudioSource::I2sIn
#define TEF_SOURCE_SINE tef::AudioSource::SineGenerator
#define TEF_OUTPUT_DAC tef::AudioOutput::Dac
#define TEF_OUTPUT_I2S tef::AudioOutput::I2sOut
