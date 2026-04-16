/**
 * @file feature_smoke_test.ino
 * @brief TEF6686 feature smoke test (pass/fail output).
 *
 * @details
 * Forces TEF6686 policy and validates expected TEF6686 behavior:
 * - FM tune and mute control should succeed
 * - FMSI APIs should return UnsupportedFeature
 * - Digital Radio APIs should return UnsupportedFeature
 */
#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
static const uint8_t kI2cAddress = 0x64;
static const uint32_t kCrystalHz = 9216000UL;

static const char* passFail(bool ok) {
  return ok ? "PASS" : "FAIL";
}

static void testResult(const char* name, bool ok) {
  Serial.print(name);
  Serial.print(": ");
  Serial.println(passFail(ok));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  radio.begin(Wire, kI2cAddress);
  radio.setVariantPolicy(TEF668X::VariantPolicy::ForceTEF6686);
  radio.setI2CSpeeds(100000UL, 100000UL);

  const TEF668X::Error initErr = radio.initAuto(kCrystalHz, false);
  if (initErr != TEF668X::Error::Ok) {
    Serial.print("initAuto: FAIL err=");
    Serial.println(static_cast<uint8_t>(initErr));
    return;
  }
  testResult("initAuto", true);
  testResult("activeVariant==TEF6686", radio.getActiveVariant() == TEF668X::Variant::TEF6686);

  TEF668X::Error e = radio.tuneFmPreset(10170);
  testResult("tuneFmPreset", e == TEF668X::Error::Ok);

  e = radio.setMute(false);
  testResult("setMute(false)", e == TEF668X::Error::Ok);

  // TEF6686 has no FMSI.
  testResult("supportsFmsi()==false", !radio.supportsFmsi());
  e = radio.setStereoImprovement(1);
  testResult("setStereoImprovement()==UnsupportedFeature", e == TEF668X::Error::UnsupportedFeature);

  // TEF6686 has no Digital Radio interface.
  testResult("supportsDigitalRadio()==false", !radio.supportsDigitalRadio());
  e = radio.setDigitalRadio(0);
  testResult("setDigitalRadio()==UnsupportedFeature", e == TEF668X::Error::UnsupportedFeature);

  uint16_t iface = 0;
  e = radio.getInterfaceStatus(iface);
  testResult("getInterfaceStatus()==UnsupportedFeature", e == TEF668X::Error::UnsupportedFeature);

  Serial.println("TEF6686 feature smoke test complete.");
}

void loop() {
  delay(1000);
}
