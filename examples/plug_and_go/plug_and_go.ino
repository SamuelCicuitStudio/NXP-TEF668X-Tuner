/*
  plug_and_go - the shortest way to a working FM radio.

  Wiring: TEF668X SDA/SCL to the board I2C pins (3.3 V levels), GPIO_1 and GPIO_2
  pulled low (address 0x64). Default board = 9.216 MHz crystal; see board_config
  if your module uses 12 MHz or other wiring.
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  radio.begin(Wire);                                // describe the board (defaults)
  if (radio.init() != tef::Error::Ok) {             // patch + start; no radio setting is changed
    Serial.println(F("TEF668X not found / init failed"));
    while (true) {}
  }
  radio.applyPreset(tef::kPresetNxpFieldFM);        // NXP recommended FM sound
  radio.applyRegion(tef::Region::Europe);           // 50 us de-emphasis
  radio.tuneFM(10470);                              // 104.70 MHz
  radio.setVolume(-100);                            // -10.0 dB
  radio.setMute(false);                             // the chip starts muted
}

void loop() {
  tef::Quality q;
  if (radio.readQuality(q) == tef::Error::Ok && q.ready()) {
    Serial.print(F("level "));
    Serial.print(q.rssi() / 10);
    Serial.print(F(" dBuV  signal "));
    Serial.print(q.signalPercent());
    Serial.println(F(" %"));
  }
  delay(1000);
}
