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
  if (radio.init() != TEF_OK) while (true) {}

  radio.applyPreset(TEF_PRESET_NXP_FIELD_AM_MW_SW);   // NXP advice for MW/SW
  // radio.applyPreset(TEF_PRESET_AM_ACTIVE_ANTENNA); // with an active antenna
  radio.setAmBandwidth(TEF_OFF, TEF_AM_BW_6KHZ);   // fixed 6 kHz (also 3 / 4 / 8 kHz)
  radio.setAmNoiseBlanker(TEF_ON, TEF_AM_NOISE_BLANKER_SENSITIVITY_DEFAULT);
  radio.setNoiseBlankerAudio(TEF_ON, TEF_AM_NOISE_BLANKER_AUDIO_SENSITIVITY_DEFAULT);

  radio.tuneAM(999);                               // 999 kHz MW
  radio.setVolumePercent(80);
  radio.setMute(TEF_OFF);
}

void loop() {
  TEFQuality q;
  if (radio.readQuality(q) == TEF_OK && q.ready()) {
    Serial.print(radio.frequency());
    Serial.print(F(" kHz  level "));
    Serial.print(q.rssi() / 10);
    Serial.print(F(" dBuV  co-channel "));
    Serial.println(q.coChannel());
  }
  if (Serial.available() && Serial.read() == 'f') radio.tuneFM(10470);  // band change
  delay(1000);
}
