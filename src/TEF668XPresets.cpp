// Preset data (see TEF668XPresets.h for the format and the meaning of each preset).
// Values are raw user-manual units; comments give the readable value.
#include "TEF668XPresets.h"

namespace tef {

const uint8_t kPresetNxpFieldFM[] TEF668X_PROGMEM = {
  TEF_CMD(FmMphSuppression, 1), TEF_W(1),                                       // iMS on
  TEF_CMD(FmChannelEqualizer, 1), TEF_W(1),                                     // channel EQ on
  TEF_CMD(FmSoftmuteTime, 4), TEF_W(120), TEF_W(500), TEF_W(10), TEF_W(20),     // fast attack 1 ms
  TEF_CMD(FmHighcutTime, 4), TEF_W(200), TEF_W(2000), TEF_W(10), TEF_W(80),     // 200 ms / 1 ms / 8 ms
  TEF_CMD(FmHighcutNoise, 3), TEF_W(3), TEF_W(150), TEF_W(200),                 // dual, 15 % / 20 %
  TEF_CMD(FmHighcutMph, 3), TEF_W(3), TEF_W(120), TEF_W(160),                   // dual, 12 % / 16 %
  TEF_CMD(FmHighcutMax, 2), TEF_W(1), TEF_W(2400),                              // 2.4 kHz (IIR)
  TEF_CMD(FmLowcutMax, 2), TEF_W(1), TEF_W(100),                                // 100 Hz
  TEF_CMD(FmStereoTime, 4), TEF_W(200), TEF_W(4000), TEF_W(20), TEF_W(80),      // 200 ms / 2 ms
  TEF_CMD(FmStereoNoise, 3), TEF_W(3), TEF_W(120), TEF_W(160),                  // 12 % / 16 %
  TEF_CMD(FmStereoMph, 3), TEF_W(3), TEF_W(100), TEF_W(150),                    // 10 % / 15 %
  TEF_CMD(FmStHiBlendNoise, 3), TEF_W(3), TEF_W(80), TEF_W(140),                // start 8 %
  TEF_CMD(FmStHiBlendMph, 3), TEF_W(3), TEF_W(80), TEF_W(140),                  // start 8 %
  TEF_END
};

const uint8_t kPresetNxpFieldAmMwSw[] TEF668X_PROGMEM = {
  TEF_CMD(AmHighcutLevel, 3), TEF_W(2), TEF_W(470), TEF_W(200),                 // 47 dBuV
  TEF_END
};

const uint8_t kPresetNxpFieldAmLw[] TEF668X_PROGMEM = {
  TEF_CMD(AmHighcutLevel, 3), TEF_W(2), TEF_W(520), TEF_W(200),                 // 52 dBuV
  TEF_CMD(AmSoftmuteLevel, 3), TEF_W(2), TEF_W(340), TEF_W(300),                // 34 dBuV / 30 dB
  TEF_CMD(AmSoftmuteMax, 2), TEF_W(1), TEF_W(330),                              // 33 dB
  TEF_END
};

const uint8_t kPresetFmCar[] TEF668X_PROGMEM = {
  TEF_CMD(FmDeemphasis, 1), TEF_W(500),                                         // 50 us
  TEF_CMD(FmBandwidth, 4), TEF_W(1), TEF_W(2360), TEF_W(1000), TEF_W(1000),     // auto, 236 kHz
  TEF_CMD(FmRfAgc, 2), TEF_W(890), TEF_W(0),                                    // 89 dBuV
  TEF_CMD(FmMphSuppression, 1), TEF_W(1),
  TEF_CMD(FmChannelEqualizer, 1), TEF_W(1),
  TEF_CMD(FmNoiseBlanker, 2), TEF_W(1), TEF_W(1000),
  TEF_CMD(FmSoftmuteTime, 4), TEF_W(120), TEF_W(500), TEF_W(10), TEF_W(20),
  TEF_CMD(FmSoftmuteLevel, 3), TEF_W(3), TEF_W(150), TEF_W(220),
  TEF_CMD(FmSoftmuteNoise, 3), TEF_W(3), TEF_W(500), TEF_W(1000),
  TEF_CMD(FmSoftmuteMph, 3), TEF_W(3), TEF_W(500), TEF_W(1000),
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(200),                              // 20 dB
  TEF_CMD(FmHighcutTime, 4), TEF_W(200), TEF_W(2000), TEF_W(10), TEF_W(80),
  TEF_CMD(FmHighcutLevel, 3), TEF_W(3), TEF_W(360), TEF_W(300),
  TEF_CMD(FmHighcutNoise, 3), TEF_W(3), TEF_W(150), TEF_W(200),
  TEF_CMD(FmHighcutMph, 3), TEF_W(3), TEF_W(120), TEF_W(160),
  TEF_CMD(FmHighcutMax, 2), TEF_W(1), TEF_W(2400),
  TEF_CMD(FmHighcutMin, 2), TEF_W(1), TEF_W(10000),                             // 10 kHz
  TEF_CMD(FmLowcutMax, 2), TEF_W(1), TEF_W(100),
  TEF_CMD(FmLowcutMin, 2), TEF_W(1), TEF_W(20),                                 // 20 Hz
  TEF_CMD(FmHighcutOptions, 1), TEF_W(1),                                       // IIR
  TEF_CMD(FmStereoTime, 4), TEF_W(200), TEF_W(4000), TEF_W(20), TEF_W(80),
  TEF_CMD(FmStereoLevel, 3), TEF_W(3), TEF_W(460), TEF_W(240),                  // 46 dBuV
  TEF_CMD(FmStereoNoise, 3), TEF_W(3), TEF_W(120), TEF_W(160),
  TEF_CMD(FmStereoMph, 3), TEF_W(3), TEF_W(100), TEF_W(150),
  TEF_CMD(FmStereoMax, 1), TEF_W(1),
  TEF_CMD(FmStHiBlendTime, 4), TEF_W(120), TEF_W(500), TEF_W(20), TEF_W(80),
  TEF_CMD(FmStHiBlendLevel, 3), TEF_W(3), TEF_W(600), TEF_W(240),
  TEF_CMD(FmStHiBlendNoise, 3), TEF_W(3), TEF_W(80), TEF_W(140),
  TEF_CMD(FmStHiBlendMph, 3), TEF_W(3), TEF_W(80), TEF_W(140),
  TEF_CMD(FmStHiBlendMax, 2), TEF_W(1), TEF_W(3000),                            // 3 kHz
  TEF_CMD(FmRds, 3), TEF_W(1), TEF_W(2), TEF_W(0),                              // decoder, auto restart
  TEF_CMD(FmScaler, 1), TEF_W(0),
  TEF_END
};

const uint8_t kPresetFmDx[] TEF668X_PROGMEM = {
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(60),                               // 6 dB
  TEF_CMD(FmHighcutMax, 2), TEF_W(1), TEF_W(3000),                              // 3 kHz
  TEF_END
};

const uint8_t kPresetAmDx[] TEF668X_PROGMEM = {
  TEF_CMD(AmBandwidth, 2), TEF_W(0), TEF_W(30),                                 // 3 kHz
  TEF_CMD(AmSoftmuteMax, 2), TEF_W(1), TEF_W(60),                               // 6 dB
  TEF_END
};

const uint8_t kPresetAmActiveAntenna[] TEF668X_PROGMEM = {
  TEF_CMD(AmAntenna, 1), TEF_W(180),                                            // 18 dB
  TEF_CMD(AmLevelOffset, 1), TEF_W(-70),                                        // -7 dB
  TEF_END
};

}  // namespace tef
