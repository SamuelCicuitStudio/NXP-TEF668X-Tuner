/*
  seek_scan - non-blocking seek and a full band scan.

  Send 'u' / 'd' on the serial monitor to seek up / down, 's' to scan the band.
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
TEF668XSeek seek(radio);

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != TEF_OK) while (true) {}
  radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);
  radio.tuneFM(8750);
  radio.setVolumePercent(70);
  radio.setMute(TEF_OFF);

  seek.setPlan(TEF_BAND_PLAN(TEF_REGION_EUROPE));       // 87.5-108 MHz, 100 kHz steps
  TEFSeekThresholds t = TEF_SEEK_FM_DEFAULT;
  t.minLevel = 250;                                        // stricter: 25 dBuV
  seek.setThresholds(TEF_FM, t);
}

void scanBand() {
  const uint16_t first = radio.frequency();
  Serial.println(F("scan:"));
  do {
    seek.start(true);
    TEFSeekStatus s;
    while ((s = seek.update()) == TEF_SEEK_RUNNING) {}
    if (s != TEF_SEEK_FOUND) break;
    Serial.println(seek.frequency());                      // 10 kHz units
  } while (seek.frequency() > first);                      // stop after wrapping
}

void loop() {
  if (Serial.available()) {
    const char c = Serial.read();
    if (c == 'u' || c == 'd') seek.start(c == 'u');
    if (c == 's') scanBand();
  }
  const TEFSeekStatus s = seek.update();   // Found is reported once
  if (s == TEF_SEEK_FOUND) {
    Serial.print(F("station at "));
    Serial.println(seek.frequency());
  } else if (s == TEF_SEEK_NOT_FOUND) {
    Serial.println(F("no station"));
  }
}
