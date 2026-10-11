/*
  am_radio - LW / MW / SW reception and switching between FM and AM.
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != tef::Error::Ok) while (true) {}

  radio.applyPreset(tef::kPresetNxpFieldAmMwSw);   // NXP advice for MW/SW
  // radio.applyPreset(tef::kPresetAmActiveAntenna); // with an active antenna
  radio.setAmBandwidth(false, 60);                 // 6 kHz (30 = 3 kHz ... 80 = 8 kHz)
  radio.setAmNoiseBlanker(true, 1000);
  radio.setNoiseBlankerAudio(true, 1000);

  radio.tuneAM(999);                               // 999 kHz MW
  radio.setVolume(-100);
  radio.setMute(false);
}

void loop() {
  tef::Quality q;
  if (radio.readQuality(q) == tef::Error::Ok && q.ready()) {
    Serial.print(radio.frequency());
    Serial.print(F(" kHz  level "));
    Serial.print(q.rssi() / 10);
    Serial.print(F(" dBuV  co-channel "));
    Serial.println(q.coChannel());
  }
  if (Serial.available() && Serial.read() == 'f') radio.tuneFM(10470);  // band change
  delay(1000);
}
