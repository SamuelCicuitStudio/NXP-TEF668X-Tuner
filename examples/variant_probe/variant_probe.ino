#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

static const char* errorToText(TEF668X::Error e) {
  switch (e) {
    case TEF668X::Error::Ok: return "Ok";
    case TEF668X::Error::NotBegun: return "NotBegun";
    case TEF668X::Error::I2CError: return "I2CError";
    case TEF668X::Error::Timeout: return "Timeout";
    case TEF668X::Error::InvalidArg: return "InvalidArg";
    case TEF668X::Error::UnsupportedFeature: return "UnsupportedFeature";
    case TEF668X::Error::BadState: return "BadState";
    case TEF668X::Error::PatchDataError: return "PatchDataError";
    default: return "UnknownError";
  }
}

static const char* variantToText(TEF668X::Variant v) {
  switch (v) {
    case TEF668X::Variant::TEF6686: return "TEF6686";
    case TEF668X::Variant::TEF6687: return "TEF6687";
    case TEF668X::Variant::TEF6688: return "TEF6688";
    case TEF668X::Variant::TEF6689: return "TEF6689";
    default: return "Unknown";
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  radio.begin(Wire, 0x64);
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

  const TEF668X::Error e = radio.initAuto(9216000UL, false);
  if (e != TEF668X::Error::Ok) {
    Serial.print("initAuto failed: ");
    Serial.println(errorToText(e));
    return;
  }

  TEF668X::Identification id;
  const TEF668X::Error idErr = radio.getIdentification(id);
  if (idErr == TEF668X::Error::Ok) {
    Serial.print("Device: 0x");
    Serial.print(id.device, HEX);
    Serial.print(" HW: 0x");
    Serial.print(id.hwVersion, HEX);
    Serial.print(" SW: 0x");
    Serial.println(id.swVersion, HEX);
  } else {
    Serial.print("getIdentification failed: ");
    Serial.println(errorToText(idErr));
  }

  Serial.print("Detected variant: ");
  Serial.println(variantToText(radio.getDetectedVariant()));
  Serial.print("Active variant: ");
  Serial.println(variantToText(radio.getActiveVariant()));
  Serial.print("Variant mismatch: ");
  Serial.println(radio.hasVariantMismatch() ? "YES" : "NO");
  Serial.print("FMSI supported: ");
  Serial.println(radio.supportsFmsi() ? "YES" : "NO");
  Serial.print("Digital Radio supported: ");
  Serial.println(radio.supportsDigitalRadio() ? "YES" : "NO");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 3000) return;
  last = millis();

  uint16_t st = 0;
  const TEF668X::Error e = radio.getOperationStatus(st);
  Serial.print("Operation status: ");
  if (e == TEF668X::Error::Ok) {
    Serial.println(st);
  } else {
    Serial.println(errorToText(e));
  }
}

