/**
 * @file fm_quality_monitor.ino
 * @brief TEF6686 FM quality monitor with CSV output.
 *
 * @details
 * Starts TEF6686 in FM mode and prints a periodic CSV line with:
 * level, USN, WAM, offset, bandwidth, modulation, signal status,
 * and processing percentages (softmute/highcut/stereo/st-hi-blend).
 *
 * Variant policy is forced to TEF6686.
 */
#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;
static const uint8_t kI2cAddress = 0x64;
static const uint32_t kCrystalHz = 9216000UL;
static const uint16_t kFmFrequency = 10170; // 101.70 MHz

static void printFixed1(int32_t value_x10) {
  if (value_x10 < 0) {
    Serial.print('-');
    value_x10 = -value_x10;
  }
  Serial.print(value_x10 / 10);
  Serial.print('.');
  Serial.print(value_x10 % 10);
}

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
  radio.begin(Wire, kI2cAddress);
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setPatchChunkSize(24);
  radio.setVariantPolicy(TEF668X::VariantPolicy::ForceTEF6686);

  if (!checkOk("initAuto", radio.initAuto(kCrystalHz, false))) return;
  if (!checkOk("applyFmDefaultProfile", radio.applyFmDefaultProfile(-80))) return;
  if (!checkOk("tuneFmPreset", radio.tuneFmPreset(kFmFrequency))) return;
  if (!checkOk("setMute", radio.setMute(false))) return;

  Serial.println("time_ms,level_dBuV,usn_pct,wam_pct,offset_kHz,bw_kHz,mod_pct,signal,softmute_pct,highcut_pct,stereo_pct,st_hi_pct");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();

  TEF668X::QualityData q;
  uint16_t signal = 0;
  TEF668X::ProcessingStatusFm p;

  if (!checkOk("getQualityFm", radio.getQualityFm(q))) return;
  if (!checkOk("getSignalStatusFm", radio.getSignalStatusFm(signal))) return;
  if (!checkOk("getProcessingStatusFm", radio.getProcessingStatusFm(p))) return;

  Serial.print(millis());
  Serial.print(',');
  printFixed1(q.level_x10_dBuV);
  Serial.print(',');
  printFixed1(q.usn_x10_pct);
  Serial.print(',');
  printFixed1(q.wam_x10_pct);
  Serial.print(',');
  printFixed1(q.offset_x10_kHz);
  Serial.print(',');
  printFixed1(q.bandwidth_x10_kHz);
  Serial.print(',');
  printFixed1(q.modulation_x10_pct);
  Serial.print(',');
  Serial.print(signal);
  Serial.print(',');
  printFixed1(p.softmute_x10_pct);
  Serial.print(',');
  printFixed1(p.highcut_x10_pct);
  Serial.print(',');
  printFixed1(p.stereo_x10_pct);
  Serial.print(',');
  printFixed1(p.stHiBlend_x10_pct);
  Serial.println();
}
