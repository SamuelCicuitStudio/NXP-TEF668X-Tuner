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
  tef::LastWrite w;
  if (radio.readLastWrite(w) != tef::Error::Ok) return;
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
  if (radio.init() != tef::Error::Ok) while (true) {}

  // L1 typed: every parameter, with enums for every option.
  radio.setFmBandwidth(true, 2360, 1000, 1000);                       // auto, 236 kHz
  radio.setFmBandwidthExt(true, 2360, 1000, 1000, 840, 2360, 300);    // + extended parameters
  radio.setStereoLevel(tef::WshMode::Dual, 460, 240);
  radio.setHighcutOptions(tef::HighcutFilter::Fir);
  radio.setStereoMin(tef::StereoMin::ForcedMono, 400);                // force mono
  printLastWrite();

  // L0 checked write by id.
  const int16_t p[] = {1, 200};
  radio.write(tef::Cmd::FmSoftmuteMax, p, 2);

  // L0 unchecked: any module / command (for experiments).
  const uint16_t raw[] = {1};
  radio.writeWords(tef::Module::FM, 20, raw, 1);                       // FM cmd 20 = multipath suppression on

  // Back to the documented chip defaults for everything on FM.
  radio.restoreChipDefaults(tef::Band::FM);

  // Raw read: index 0 returns a confirmation word first (user manual 5.7.2).
  uint16_t w[2];
  radio.readWords(tef::Module::Appl, 128, w, 2, 0);
  Serial.print(F("confirmation 0x"));
  Serial.print(w[0], HEX);                                             // 0x4080 = APPL cmd 128
  Serial.print(F(", state "));
  Serial.println(w[1]);

  radio.tuneFM(10470);
  radio.setMute(false);
}

void loop() {
  tef::ProcessingStatus ps;
  if (radio.readProcessing(ps) == tef::Error::Ok) {
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
