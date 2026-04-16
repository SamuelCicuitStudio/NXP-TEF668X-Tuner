#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
static const uint16_t kAmFrequency = 999; // 999 kHz (MW)

static bool checkOk(const char* label, TEF668X::Error e) {
  if (e == TEF668X::Error::Ok) return true;
  Serial.print(label);
  Serial.print(" failed, err=");
  Serial.println(static_cast<uint8_t>(e));
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  radio.begin(Wire, 0x64);
  radio.setI2CSpeeds(100000UL, 100000UL);

  if (!checkOk("initAuto", radio.initAuto(9216000UL, false))) return;
  if (!checkOk("setVolumeDb10", radio.setVolumeDb10(-100))) return; // -10.0 dB
  if (!checkOk("tuneAmPreset", radio.tuneAmPreset(kAmFrequency))) return;
  if (!checkOk("setMute", radio.setMute(false))) return;

  Serial.print("AM tuned to ");
  Serial.print(kAmFrequency);
  Serial.println(" kHz");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();

  uint16_t status = 0;
  const TEF668X::Error e = radio.getOperationStatus(status);
  Serial.print("OpState=");
  if (e == TEF668X::Error::Ok) {
    Serial.println(status);
  } else {
    Serial.print("err ");
    Serial.println(static_cast<uint8_t>(e));
  }
}

