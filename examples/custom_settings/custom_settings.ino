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
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(120),       // SoftMute at most 12 dB
  TEF_CMD(FmStereoMin, 2), TEF_W(1), TEF_W(200),         // keep at least 20 dB stereo separation
  TEF_CMD(FmDeemphasis, 1), TEF_W(tef::Deemphasis::Us75),// USA
  TEF_END
};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != tef::Error::Ok) while (true) {}
  radio.attachShadow(&shadow);

  radio.applyPreset(tef::kPresetNxpFieldFM);
  if (radio.applyPreset(myPreset) != tef::Error::Ok) {
    Serial.print(F("preset stopped at entry "));
    Serial.println(radio.lastPresetEntry());
  }

  // HighCut: start from the chip defaults, change two things, write all of it.
  tef::WshConfig hc;
  radio.wshDefaults(tef::WshFunction::Highcut, tef::Band::FM, hc);
  hc.level.start = 300;                 // start cutting treble below 30 dBuV (default 36)
  hc.max.limit = 3000;                  // strongest cut: treble down to 3 kHz (default 4 kHz)
  radio.setWsh(tef::WshFunction::Highcut, tef::Band::FM, hc, tef::kWshLevel | tef::kWshMax);

  // Range checking: this is refused and nothing is sent.
  const tef::Error e = radio.setSoftmuteMax(tef::Band::FM, true, 900);  // limit max is 400
  if (e == tef::Error::OutOfRange) {
    Serial.print(F("refused parameter #"));
    Serial.println(radio.lastErrorParam());     // 1 = limit
  }

  // What was written last for SoftMute max (shadow):
  int16_t p[7];
  uint8_t n;
  if (shadow.get(tef::Cmd::FmSoftmuteMax, p, n)) {
    Serial.print(F("SoftMute max limit = "));
    Serial.println(p[1]);                        // 120 from myPreset
  }

  // Constants for every range and default are generated too:
  Serial.print(F("SoftMute level default start: "));
  Serial.println(tef::FmSoftmuteLevel::kStartDefault);  // 150 = 15.0 dBuV

  radio.tuneFM(10470);
  radio.setVolume(-100);
  radio.setMute(false);
}

void loop() {}
