/*
  deep_control - full access for experts.

  L1: one typed function per user-manual command (all parameters, range checked).
  L0: write() with a command id, or writeWords()/readWords() without any check.
  Debug: readLastWrite() shows exactly what the chip received.
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void printLastWrite() {
  TEFLastWrite w;
  if (radio.readLastWrite(w) != TEF_OK) return;
  Serial.print(F("chip received: module "));
  Serial.print(w.module);
  Serial.print(F(" cmd "));
  Serial.print(w.cmd);
  for (uint8_t i = 0; i < w.paramCount && i < 5; ++i) {
    Serial.print(' ');
    Serial.print(static_cast<int16_t>(w.params[i]));
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  radio.begin(Wire);
  if (radio.init() != TEF_OK) while (true) {}

  // L1 typed: every parameter, with enums for every option.
  radio.setFmBandwidth(TEF_ON, TEF_FM_BW_236KHZ, 1000, 1000);         // auto, 236 kHz
  radio.setFmBandwidthExt(TEF_ON, TEF_FM_BW_236KHZ, 1000, 1000, 840, 2360, 300);  // + extended parameters
  radio.setStereoLevel(TEF_WSH_DUAL, 460, 240);
  radio.setHighcutOptions(TEF_HIGHCUT_FIR);
  radio.setStereoMin(TEF_STEREO_FORCED_MONO, 400);                // force mono
  printLastWrite();

  // L0 checked write by id.
  const int16_t p[] = {1, 200};
  radio.write(TEFCmd::FmSoftmuteMax, p, 2);

  // L0 unchecked: any module / command (for experiments).
  const uint16_t raw[] = {1};
  radio.writeWords(TEF_MODULE_FM, 20, raw, 1);                       // FM cmd 20 = multipath suppression on

  // Back to the documented chip defaults for everything on FM.
  radio.restoreChipDefaults(TEF_FM);

  // Raw read: index 0 returns a confirmation word first (user manual 5.7.2).
  uint16_t w[2];
  radio.readWords(TEF_MODULE_APPL, 128, w, 2, 0);
  Serial.print(F("confirmation 0x"));
  Serial.print(w[0], HEX);                                             // 0x4080 = APPL cmd 128
  Serial.print(F(", state "));
  Serial.println(w[1]);

  radio.tuneFM(10470);
  radio.setMute(TEF_OFF);
}

void loop() {
  TEFProcessing ps;
  if (radio.readProcessing(ps) == TEF_OK) {
    Serial.print(F("softmute "));
    Serial.print(ps.softmute / 10);
    Serial.print(F("%  highcut "));
    Serial.print(ps.highcut / 10);
    Serial.print(F("%  mono "));
    Serial.print(ps.stereo / 10);
    Serial.println('%');
  }
  delay(1000);
}
