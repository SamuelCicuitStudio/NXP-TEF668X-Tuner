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
  if (radio.init() != tef::Error::Ok) while (true) {}
  radio.applyPreset(tef::kPresetNxpFieldFM);
  radio.tuneFM(8750);
  radio.setVolume(-150);
  radio.setMute(false);

  seek.setPlan(tef::bandPlan(tef::Region::Europe));       // 87.5-108 MHz, 100 kHz steps
  tef::SeekThresholds t = tef::kSeekFmDefault;
  t.minLevel = 250;                                        // stricter: 25 dBuV
  seek.setThresholds(tef::Band::FM, t);
}

void scanBand() {
  const uint16_t first = radio.frequency();
  Serial.println(F("scan:"));
  do {
    seek.start(true);
    TEF668XSeek::Status s;
    while ((s = seek.update()) == TEF668XSeek::Status::Running) {}
    if (s != TEF668XSeek::Status::Found) break;
    Serial.println(seek.frequency());                      // 10 kHz units
  } while (seek.frequency() > first);                      // stop after wrapping
}

void loop() {
  if (Serial.available()) {
    const char c = Serial.read();
    if (c == 'u' || c == 'd') seek.start(c == 'u');
    if (c == 's') scanBand();
  }
  const TEF668XSeek::Status s = seek.update();   // Found is reported once
  if (s == TEF668XSeek::Status::Found) {
    Serial.print(F("station at "));
    Serial.println(seek.frequency());
  } else if (s == TEF668XSeek::Status::NotFound) {
    Serial.println(F("no station"));
  }
}
