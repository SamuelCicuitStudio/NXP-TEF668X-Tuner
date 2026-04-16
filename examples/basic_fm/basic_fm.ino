/**
 * @file basic_fm.ino
 * @brief TEF6686 baseline FM bring-up example (car profile).
 *
 * @details
 * This example is TEF6686-focused and follows the same bring-up lead as
 * `ref/tef6686done.ino`:
 * - 9.216 MHz crystal reference
 * - low I2C speed for patch/runtime stability
 * - TEF6686 car-oriented FM profile
 * - tune + unmute + periodic quality readback
 *
 * Variant policy is forced to TEF6686 for predictable behavior:
 * `TEF668X::VariantPolicy::ForceTEF6686`.
 */
#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
static const uint8_t kI2cAddress = 0x64;
static const uint32_t kCrystalHz = 9216000UL; // 9.216 MHz
static const uint16_t kDefaultFreqFm_x10kHz = 10470; // 104.70 MHz
static const int16_t kDefaultVolume_x10dB = -80; // -8.0 dB

static void printError(const char* label, TEF668X::Error e) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(static_cast<uint8_t>(e));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  radio.begin(Wire, kI2cAddress);
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setPatchChunkSize(24);
  radio.setVariantPolicy(TEF668X::VariantPolicy::ForceTEF6686);

  TEF668X::Error e = radio.initAuto(kCrystalHz, false);
  if (e != TEF668X::Error::Ok) {
    printError("initAuto failed", e);
    return;
  }

  e = radio.applyFmCarProfile(kDefaultVolume_x10dB);
  if (e != TEF668X::Error::Ok) {
    printError("applyFmCarProfile failed", e);
    return;
  }

  e = radio.tuneFmPreset(kDefaultFreqFm_x10kHz);
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
