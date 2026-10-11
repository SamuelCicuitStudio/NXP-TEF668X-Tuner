#pragma once
/**
 * @file TEF668XNames.h
 * @brief Short uppercase names for everything a sketch passes to the library,
 *        so user code never needs `tef::Something::Value`.
 *
 * - Values:  TEF_<GROUP>_<VALUE>   e.g. TEF_FM, TEF_WSH_DUAL, TEF_PRESET_FM_CAR, TEF_OK
 * - Types:   TEF<Name>             e.g. TEFBoard, TEFQuality, TEFWshConfig
 * - Common values in plain units: TEF_AM_BW_6KHZ, TEF_VOLUME_0DB, TEF_ANTENNA_18DB ...
 * - Chip defaults of every parameter: TEF668XDefaults.h (TEF_<COMMAND>_<PARAM>_DEFAULT)
 *
 * Each name is another spelling of a typed value, so the compiler still checks it
 * (TEF_FM where a preset is expected is an error). The tef:: style keeps working.
 * Included automatically by TEF668X.h.
 */

#include "TEF668XDefaults.h"

// ============================================================================
// Types (use them to declare variables)
// ============================================================================
typedef tef::Error TEFError;                    ///< result of every call
typedef tef::BoardConfig TEFBoard;              ///< hardware description for begin()
typedef tef::GpioWiring TEFGpioWiring;          ///< one TEF GPIO in TEFBoard
typedef tef::Quality TEFQuality;                ///< readQuality()
typedef tef::SignalStatus TEFSignal;            ///< readSignal()
typedef tef::ProcessingStatus TEFProcessing;    ///< readProcessing()
typedef tef::AgcStatus TEFAgc;                  ///< readAgc()
typedef tef::Identification TEFIdentification;  ///< readIdentification()
typedef tef::LastWrite TEFLastWrite;            ///< readLastWrite()
typedef tef::RdsGroup TEFRdsGroup;              ///< readRds()
typedef tef::WshConfig TEFWshConfig;            ///< setWsh() / wshDefaults()
typedef tef::BandPlan TEFBandPlan;              ///< TEF_BAND_PLAN(region)
typedef tef::SeekThresholds TEFSeekThresholds;  ///< TEF668XSeek::setThresholds()
typedef tef::RdsPolicyConfig TEFRdsPolicyConfig;///< TEF668XRds::setCustomPolicy()
typedef tef::RdsClock TEFRdsClock;              ///< TEF668XRds::clock()
typedef tef::Cmd TEFCmd;                        ///< command id (raw write)
typedef TEF668XSeek::Status TEFSeekStatus;      ///< TEF668XSeek::update()

/// Band plan (limits, steps, de-emphasis) of a region: TEF_BAND_PLAN(TEF_REGION_EUROPE).
#define TEF_BAND_PLAN(region) tef::bandPlan(region)

// ============================================================================
// Results
// ============================================================================
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

// ============================================================================
// On / off (every `enable` / `autoMode` / `master` / `standby` parameter)
// ============================================================================
#define TEF_ON true
#define TEF_OFF false

// ============================================================================
// Start-up, chip identity, state
// ============================================================================
#define TEF_INIT_AUTO TEF668X::InitMode::Auto        ///< patch only when needed (normal)
#define TEF_INIT_PATCH TEF668X::InitMode::Patch      ///< always reload the patch
#define TEF_INIT_NO_PATCH TEF668X::InitMode::NoPatch ///< never upload the patch

#define TEF_VARIANT_UNKNOWN tef::Variant::Unknown
#define TEF_VARIANT_6686 tef::Variant::TEF6686
#define TEF_VARIANT_6687 tef::Variant::TEF6687       ///< + FMSI
#define TEF_VARIANT_6688 tef::Variant::TEF6688       ///< + digital radio
#define TEF_VARIANT_6689 tef::Variant::TEF6689       ///< + FMSI + digital radio

#define TEF_STATE_BOOT tef::OpState::Boot
#define TEF_STATE_IDLE tef::OpState::Idle
#define TEF_STATE_STANDBY tef::OpState::Standby
#define TEF_STATE_FM tef::OpState::ActiveFM
#define TEF_STATE_AM tef::OpState::ActiveAM

// ============================================================================
// Board description (TEFBoard)
// ============================================================================
#define TEF_CLOCK_4MHZ tef::kClock4MHz            ///< crystal (no digital radio)
#define TEF_CLOCK_9216KHZ tef::kClock9216kHz      ///< crystal, default
#define TEF_CLOCK_12MHZ tef::kClock12MHz          ///< crystal
#define TEF_CLOCK_55466KHZ tef::kClock55466kHz    ///< external clock only
#define TEF_ADDRESS_LOW tef::kI2cAddressLow       ///< 0x64, GPIO_2 low (default)
#define TEF_ADDRESS_HIGH tef::kI2cAddressHigh     ///< 0x65, GPIO_2 high
#define TEF_POWER_SHARED tef::TunerPower::SharedWithMcu
#define TEF_POWER_SWITCHED tef::TunerPower::Switched
#define TEF_POWER_ENABLE_PIN tef::TunerPower::EnablePin
#define TEF_GPIO_UNUSED tef::GpioFeature::Unused
#define TEF_GPIO_INPUT tef::GpioFeature::Input    ///< GPIO_0 only
#define TEF_GPIO_LOW tef::GpioFeature::Low
#define TEF_GPIO_HIGH tef::GpioFeature::High
#define TEF_GPIO_RDS_DAVN tef::GpioFeature::Davn  ///< RDS data waiting (active low)
#define TEF_GPIO_QSI tef::GpioFeature::Qsi        ///< quality ready after tune (active low)
#define TEF_GPIO_QSI_DAVN tef::GpioFeature::QsiDavn
#define TEF_GPIO_RDDA tef::GpioFeature::Rdda      ///< legacy RDS data
#define TEF_GPIO_RDCL tef::GpioFeature::Rdcl      ///< legacy RDS clock
#define TEF_GPIO_AGC_STEP tef::GpioFeature::AgcStep

// ============================================================================
// Bands, regions, tuning
// ============================================================================
#define TEF_FM tef::Band::FM
#define TEF_AM tef::Band::AM
#define TEF_BAND_FM tef::Band::FM
#define TEF_BAND_AM tef::Band::AM

#define TEF_MODULE_FM tef::Module::FM             ///< raw writeWords / readWords
#define TEF_MODULE_AM tef::Module::AM
#define TEF_MODULE_AUDIO tef::Module::Audio
#define TEF_MODULE_APPL tef::Module::Appl

#define TEF_REGION_EUROPE tef::Region::Europe     ///< 87.5-108 MHz / 100 kHz, 50 us
#define TEF_REGION_USA tef::Region::USA           ///< 87.5-107.9 MHz / 200 kHz, 75 us
#define TEF_REGION_JAPAN tef::Region::Japan       ///< 76-95 MHz / 100 kHz, 50 us
#define TEF_REGION_OIRT tef::Region::Oirt         ///< 65.9-74 MHz / 30 kHz, 50 us

#define TEF_TUNE_PRESET tef::TuneMode::Preset     ///< tune, short mute (normal)
#define TEF_TUNE_SEARCH tef::TuneMode::Search     ///< tune and stay muted
#define TEF_TUNE_AF_UPDATE tef::TuneMode::AfUpdate///< measure an AF inaudibly
#define TEF_TUNE_JUMP tef::TuneMode::Jump         ///< move to an AF, inaudible
#define TEF_TUNE_CHECK tef::TuneMode::Check       ///< move to an AF, stay muted
#define TEF_TUNE_END tef::TuneMode::End           ///< release the mute

#define TEF_FM_MIN tef::kFmMin                    ///< 6500 = 65.00 MHz
#define TEF_FM_MAX tef::kFmMax                    ///< 10800 = 108.00 MHz
#define TEF_LW_MIN tef::kLwMin                    ///< 144 kHz
#define TEF_LW_MAX tef::kLwMax                    ///< 288 kHz
#define TEF_MW_MIN tef::kMwMin                    ///< 522 kHz
#define TEF_MW_MAX tef::kMwMax                    ///< 1710 kHz
#define TEF_SW_MIN tef::kSwMin                    ///< 2300 kHz
#define TEF_SW_MAX tef::kSwMax                    ///< 27000 kHz

#define TEF_DEEMPHASIS_OFF tef::Deemphasis::Off
#define TEF_DEEMPHASIS_50US tef::Deemphasis::Us50 ///< Europe, Asia
#define TEF_DEEMPHASIS_75US tef::Deemphasis::Us75 ///< Americas

#define TEF_RESTART_NONE tef::Restart::None
#define TEF_RESTART_NOW tef::Restart::Now
#define TEF_RESTART_AFTER_TUNE tef::Restart::AfterTune

#define TEF_QSI_PIN_NONE tef::QsiPin::None
#define TEF_QSI_PIN_GPIO tef::QsiPin::Gpio

// ============================================================================
// Presets
// ============================================================================
#define TEF_PRESET_NXP_FIELD_FM tef::kPresetNxpFieldFM            ///< NXP recommended FM values
#define TEF_PRESET_NXP_FIELD_AM_MW_SW tef::kPresetNxpFieldAmMwSw  ///< NXP recommended AM MW/SW values
#define TEF_PRESET_NXP_FIELD_AM_LW tef::kPresetNxpFieldAmLw       ///< NXP recommended AM LW values
#define TEF_PRESET_FM_CAR tef::kPresetFmCar                       ///< complete FM car setup
#define TEF_PRESET_FM_DX tef::kPresetFmDx                         ///< FM long-distance listening
#define TEF_PRESET_AM_DX tef::kPresetAmDx                         ///< AM long-distance listening
#define TEF_PRESET_AM_ACTIVE_ANTENNA tef::kPresetAmActiveAntenna  ///< AM with an active antenna

// ============================================================================
// Weak-signal handling
// ============================================================================
#define TEF_WSH_OFF tef::WshMode::Off
#define TEF_WSH_FAST tef::WshMode::Fast
#define TEF_WSH_SLOW tef::WshMode::Slow
#define TEF_WSH_DUAL tef::WshMode::Dual

#define TEF_SOFTMUTE tef::WshFunction::Softmute
#define TEF_HIGHCUT tef::WshFunction::Highcut
#define TEF_STEREO tef::WshFunction::Stereo
#define TEF_STHIBLEND tef::WshFunction::StHiBlend

#define TEF_WSH_PART_TIME tef::kWshTime           ///< parts for setWsh(), combine with |
#define TEF_WSH_PART_LEVEL tef::kWshLevel
#define TEF_WSH_PART_NOISE tef::kWshNoise
#define TEF_WSH_PART_MPH tef::kWshMph
#define TEF_WSH_PART_MOD tef::kWshMod
#define TEF_WSH_PART_MAX tef::kWshMax
#define TEF_WSH_PART_MIN tef::kWshMin
#define TEF_WSH_PART_ALL tef::kWshAll

#define TEF_HIGHCUT_IIR tef::HighcutFilter::Iir
#define TEF_HIGHCUT_DEEMPHASIS tef::HighcutFilter::Deemphasis
#define TEF_HIGHCUT_FIR tef::HighcutFilter::Fir

#define TEF_STEREO_MIN_OFF tef::StereoMin::Off
#define TEF_STEREO_MIN_LIMIT tef::StereoMin::Limit
#define TEF_STEREO_FORCED_MONO tef::StereoMin::ForcedMono

// ============================================================================
// RDS
// ============================================================================
#define TEF_RDS_OFF tef::RdsMode::Off
#define TEF_RDS_DECODER tef::RdsMode::Decoder
#define TEF_RDS_DEMODULATOR tef::RdsMode::Demodulator
#define TEF_RDS_PIN_NONE tef::RdsPin::None
#define TEF_RDS_PIN_DAVN tef::RdsPin::Davn
#define TEF_RDS_PIN_LEGACY tef::RdsPin::Legacy2Wire
#define TEF_RDS_STRICT tef::RdsPolicy::Strict
#define TEF_RDS_NORMAL tef::RdsPolicy::Normal
#define TEF_RDS_FAST tef::RdsPolicy::Fast
#define TEF_RDS_DX tef::RdsPolicy::Dx
#define TEF_RDS_CUSTOM tef::RdsPolicy::Custom

// ============================================================================
// Seek
// ============================================================================
#define TEF_SEEK_IDLE TEF668XSeek::Status::Idle
#define TEF_SEEK_RUNNING TEF668XSeek::Status::Running
#define TEF_SEEK_FOUND TEF668XSeek::Status::Found
#define TEF_SEEK_NOT_FOUND TEF668XSeek::Status::NotFound
#define TEF_SEEK_FAILED TEF668XSeek::Status::Failed
#define TEF_SEEK_FM_DEFAULT tef::kSeekFmDefault   ///< 20 dBuV, 25 % noise/multipath, 10 kHz
#define TEF_SEEK_AM_DEFAULT tef::kSeekAmDefault   ///< 30 dBuV, 1.5 kHz

// ============================================================================
// Audio
// ============================================================================
#define TEF_SOURCE_RADIO tef::AudioSource::Radio
#define TEF_SOURCE_ANALOG_RADIO tef::AudioSource::AnalogRadio
#define TEF_SOURCE_I2S_IN tef::AudioSource::I2sIn
#define TEF_SOURCE_AUDIO_PROCESSOR tef::AudioSource::AudioProcessor
#define TEF_SOURCE_SINE tef::AudioSource::SineGenerator
#define TEF_OUTPUT_DAC tef::AudioOutput::Dac
#define TEF_OUTPUT_I2S tef::AudioOutput::I2sOut

#define TEF_I2S_INPUT tef::I2sSignal::Input
#define TEF_I2S_OUTPUT tef::I2sSignal::Output
#define TEF_I2S_MODE_OFF tef::I2sMode::Off
#define TEF_I2S_MODE_INPUT tef::I2sMode::Input
#define TEF_I2S_MODE_OUTPUT tef::I2sMode::Output
#define TEF_I2S_FORMAT_16 tef::I2sFormat::I2s16
#define TEF_I2S_FORMAT_32 tef::I2sFormat::I2s32
#define TEF_I2S_FORMAT_LSB16 tef::I2sFormat::Lsb16
#define TEF_I2S_FORMAT_LSB18 tef::I2sFormat::Lsb18
#define TEF_I2S_FORMAT_LSB20 tef::I2sFormat::Lsb20
#define TEF_I2S_FORMAT_LSB24 tef::I2sFormat::Lsb24
#define TEF_I2S_RATE_32K tef::I2sRate::Rate32k
#define TEF_I2S_RATE_44K1 tef::I2sRate::Rate44k1
#define TEF_I2S_RATE_48K tef::I2sRate::Rate48k

#define TEF_WAVE_OFF tef::WaveMode::Off
#define TEF_WAVE_1_LEFT tef::WaveMode::Wave1Left
#define TEF_WAVE_2_RIGHT tef::WaveMode::Wave2Right
#define TEF_WAVE_BOTH tef::WaveMode::Both
#define TEF_WAVE_1_STEREO tef::WaveMode::Wave1Stereo
#define TEF_WAVE_2_STEREO tef::WaveMode::Wave2Stereo
#define TEF_WAVE_SUM tef::WaveMode::Sum

#define TEF_SPECIAL_NORMAL tef::AnaOutSpecial::Normal
#define TEF_SPECIAL_MPX tef::AnaOutSpecial::Mpx           ///< DAC_L = FM MPX (DARC), DAC_R = mono
#define TEF_SPECIAL_DIGITAL_ANALOG tef::AnaOutSpecial::DigitalAnalog

// ============================================================================
// Digital radio (TEF6688 / TEF6689)
// ============================================================================
#define TEF_DR_BLEND_PIN tef::DrBlend::Pin
#define TEF_DR_BLEND_PIN_INVERTED tef::DrBlend::PinInverted
#define TEF_DR_BLEND_DIGITAL tef::DrBlend::ForceDigital
#define TEF_DR_BLEND_ANALOG tef::DrBlend::ForceAnalog

// ============================================================================
// Common values in plain units (raw API units underneath)
// ============================================================================
// Volume, setVolume() (0.1 dB)
#define TEF_VOLUME_MUTE (-599)      ///< -59.9 dB, the chip mutes at -60 dB
#define TEF_VOLUME_MINUS_20DB (-200)
#define TEF_VOLUME_MINUS_10DB (-100)
#define TEF_VOLUME_0DB 0            ///< chip default, no gain
#define TEF_VOLUME_MAX 240          ///< +24 dB (may clip)

// AM IF bandwidth, setAmBandwidth() (0.1 kHz)
#define TEF_AM_BW_3KHZ 30           ///< narrow, crowded band / DX
#define TEF_AM_BW_4KHZ 40           ///< chip default
#define TEF_AM_BW_6KHZ 60           ///< normal listening
#define TEF_AM_BW_8KHZ 80           ///< wide, strong stations

// FM IF bandwidth, setFmBandwidth() (0.1 kHz)
#define TEF_FM_BW_56KHZ 560         ///< narrowest
#define TEF_FM_BW_114KHZ 1140
#define TEF_FM_BW_236KHZ 2360       ///< chip default
#define TEF_FM_BW_311KHZ 3110       ///< widest

// AM antenna attenuation, setAntenna() (0.1 dB, 6 dB steps)
#define TEF_ANTENNA_0DB 0           ///< passive antenna, chip default
#define TEF_ANTENNA_6DB 60
#define TEF_ANTENNA_12DB 120
#define TEF_ANTENNA_18DB 180        ///< active antenna (NXP advice)
#define TEF_ANTENNA_24DB 240        ///< active antenna (NXP advice)
#define TEF_ANTENNA_30DB 300
#define TEF_ANTENNA_36DB 360

// FM RF AGC start, setFmRfAgc() (0.1 dBuV)
#define TEF_FM_AGC_84DBUV 840       ///< earliest: best near strong transmitters
#define TEF_FM_AGC_89DBUV 890       ///< NXP example
#define TEF_FM_AGC_92DBUV 920       ///< chip default, best sensitivity
