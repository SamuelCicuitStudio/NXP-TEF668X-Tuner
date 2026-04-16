#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

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
  radio.begin(Wire, 0x64);
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);
  radio.setI2CSpeeds(100000UL, 100000UL);

  const TEF668X::Error initErr = radio.initAuto(9216000UL, false);
  if (initErr != TEF668X::Error::Ok) {
    Serial.print("initAuto: FAIL err=");
    Serial.println(static_cast<uint8_t>(initErr));
    return;
  }
  testResult("initAuto", true);

  TEF668X::Error e = radio.tuneFmPreset(10170);
  testResult("tuneFmPreset", e == TEF668X::Error::Ok);

  e = radio.setMute(false);
  testResult("setMute(false)", e == TEF668X::Error::Ok);

  // FMSI feature behavior should match variant capabilities.
  const bool fmsiSupported = radio.supportsFmsi();
  e = radio.setStereoImprovement(1);
  testResult("setStereoImprovement()", fmsiSupported ? (e == TEF668X::Error::Ok)
                                                     : (e == TEF668X::Error::UnsupportedFeature));

  // Digital Radio feature behavior should match variant capabilities.
  const bool drSupported = radio.supportsDigitalRadio();
  e = radio.setDigitalRadio(0);
  testResult("setDigitalRadio()", drSupported ? (e == TEF668X::Error::Ok)
                                              : (e == TEF668X::Error::UnsupportedFeature));

  uint16_t iface = 0;
  e = radio.getInterfaceStatus(iface);
  testResult("getInterfaceStatus()", drSupported ? (e == TEF668X::Error::Ok)
                                                 : (e == TEF668X::Error::UnsupportedFeature));

  Serial.println("Feature smoke test complete.");
}

void loop() {
  delay(1000);
}

