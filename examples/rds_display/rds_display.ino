/*
  rds_display - station name, RadioText, PTY and clock with a selectable error policy.

  Policies: Strict (clean, slower), Normal (default), Fast (instant, may flicker),
  Dx (weak signals), or Custom (your own numbers).
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
TEF668XRds rds(radio);

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != tef::Error::Ok) while (true) {}
  radio.applyPreset(tef::kPresetNxpFieldFM);
  radio.setRds(tef::RdsMode::Decoder, tef::Restart::AfterTune, tef::RdsPin::None);  // chip default, made explicit
  radio.tuneFM(10470);
  radio.setVolume(-100);
  radio.setMute(false);

  rds.setPolicy(tef::RdsPolicy::Normal);
  // Custom example: accept small errors, show PS after 3 identical receptions, RT after 2.
  // rds.setCustomPolicy({1, 3, 2});
}

void loop() {
  rds.update();  // reads every buffered group; call at least every 80 ms

  if (rds.psChanged()) {
    Serial.print(F("PS: "));
    Serial.println(rds.ps());
  }
  if (rds.rtChanged() && rds.rtComplete()) {
    Serial.print(F("RT: "));
    Serial.println(rds.rt());
  }

  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.print(F("PI "));
    Serial.print(rds.pi(), HEX);
    Serial.print(F("  PTY "));
    Serial.print(rds.pty());
    Serial.print(F("  synced "));
    Serial.print(rds.synced());
    Serial.print(F("  quality "));
    Serial.print(rds.quality());
    Serial.print(F("%  AF "));
    Serial.println(rds.afCount());
    if (rds.clock().valid) {
      Serial.print(F("UTC "));
      Serial.print(rds.clock().hour);
      Serial.print(':');
      Serial.println(rds.clock().minute);
    }
  }
  delay(20);
}
