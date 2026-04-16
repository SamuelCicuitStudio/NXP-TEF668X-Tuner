#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

static void printError(const char* label, TEF668X::Error e) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(static_cast<uint8_t>(e));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  radio.begin(Wire, 0x64);
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setPatchChunkSize(24);
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

  TEF668X::Error e = radio.initAuto(9216000UL, false);
  if (e != TEF668X::Error::Ok) {
    printError("initAuto failed", e);
    return;
  }

  e = radio.applyFmCarProfile(-80); // -8.0 dB
  if (e != TEF668X::Error::Ok) {
    printError("applyFmCarProfile failed", e);
    return;
  }

  e = radio.tuneFmPreset(10470); // 104.70 MHz
  if (e != TEF668X::Error::Ok) {
    printError("tuneFmPreset failed", e);
    return;
  }

  e = radio.setMute(false);
  if (e != TEF668X::Error::Ok) {
    printError("setMute failed", e);
    return;
  }

  TEF668X::Identification id;
  if (radio.getIdentification(id) == TEF668X::Error::Ok) {
    Serial.print("Device: 0x");
    Serial.print(id.device, HEX);
    Serial.print(" HW: 0x");
    Serial.print(id.hwVersion, HEX);
    Serial.print(" SW: 0x");
    Serial.println(id.swVersion, HEX);
  }

  if (radio.supportsFmsi()) {
    radio.setStereoImprovement(1);
  }

  if (radio.supportsDigitalRadio()) {
    // Example only: enable DR path if your board is wired for it.
    radio.setDigitalRadio(1);
  }
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    TEF668X::QualityData q;
    if (radio.getQualityFm(q) == TEF668X::Error::Ok) {
      Serial.print("Level dBuV: ");
      Serial.print(q.level_x10_dBuV / 10.0f);
      Serial.print(" USN%: ");
      Serial.print(q.usn_x10_pct / 10.0f);
      Serial.print(" WAM%: ");
      Serial.println(q.wam_x10_pct / 10.0f);
    }
  }
}
