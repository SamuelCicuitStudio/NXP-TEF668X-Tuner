/*
  custom_settings - go deeper without writing raw commands.

  1. Layer presets: NXP base + your own preset (same format as the built-in ones).
  2. Change one weak-signal function as a whole with WshConfig.
  3. Keep a copy of what was written with TEF668XShadow (optional, ~580 bytes RAM).
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
TEF668XShadow shadow;

// Own preset: entries = TEF_CMD(command, parameter count) followed by the words.
const uint8_t myPreset[] TEF668X_PROGMEM = {
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(TEF_ON), TEF_W(120),                  // SoftMute at most 12 dB
  TEF_CMD(FmStereoMin, 2), TEF_W(TEF_STEREO_MIN_LIMIT), TEF_W(200),     // keep >= 20 dB stereo separation
  TEF_CMD(FmDeemphasis, 1), TEF_W(TEF_DEEMPHASIS_75US),                  // USA
  TEF_END
};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != TEF_OK) while (true) {}
  radio.attachShadow(&shadow);

  radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);
  if (radio.applyPreset(myPreset) != TEF_OK) {
    Serial.print(F("preset stopped at entry "));
    Serial.println(radio.lastPresetEntry());
  }

  // HighCut: start from the chip defaults, change two things, write all of it.
  TEFWshConfig hc;
  radio.wshDefaults(TEF_HIGHCUT, TEF_FM, hc);
  hc.level.start = 300;                 // start cutting treble below 30 dBuV (default 36)
  hc.max.limit = 3000;                  // strongest cut: treble down to 3 kHz (default 4 kHz)
  radio.setWsh(TEF_HIGHCUT, TEF_FM, hc, TEF_WSH_PART_LEVEL | TEF_WSH_PART_MAX);

  // Range checking: this is refused and nothing is sent.
  const TEFError e = radio.setSoftmuteMax(TEF_FM, TEF_ON, 900);  // limit max is 400
  if (e == TEF_ERR_OUT_OF_RANGE) {
    Serial.print(F("refused parameter #"));
    Serial.println(radio.lastErrorParam());     // 1 = limit
  }

  // What was written last for SoftMute max (shadow):
  int16_t p[7];
  uint8_t n;
  if (shadow.get(TEFCmd::FmSoftmuteMax, p, n)) {
    Serial.print(F("SoftMute max limit = "));
    Serial.println(p[1]);                        // 120 from myPreset
  }

  // Constants for every range and default are generated too:
  Serial.print(F("SoftMute level default start: "));
  Serial.println(TEF_FM_SOFTMUTE_LEVEL_START_DEFAULT);  // 150 = 15.0 dBuV

  radio.tuneFM(10470);
  radio.setVolumePercent(80);
  radio.setMute(TEF_OFF);
}

void loop() {}
