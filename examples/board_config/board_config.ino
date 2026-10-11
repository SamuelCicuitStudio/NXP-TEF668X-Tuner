/*
  board_config - describe YOUR hardware once; the library adapts to it.

  Every field of tef::BoardConfig is optional; change only what differs from the
  defaults (9.216 MHz crystal, address 0x64, FM + AM, shared supply, no GPIO used).
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  tef::BoardConfig board;
  board.clockHz = tef::kClock12MHz;            // crystal: kClock4MHz / kClock9216kHz / kClock12MHz
  board.externalClock = false;                 // true + kClock55466kHz for an external clock
  board.i2cAddress = tef::kI2cAddressLow;      // GPIO_2 low = 0x64, high = 0x65
  board.fm = true;                             // bands used by the product:
  board.am = false;                            //   AM commands now return Error::BandDisabled

  board.power = tef::TunerPower::EnablePin;    // SharedWithMcu / Switched / EnablePin
  board.powerPin = 7;                          // MCU pin that switches the tuner supply
  board.powerActiveHigh = true;
  board.powerUpDelayMs = 10;                   // no I2C during the first 5 ms (AN11254 2.8)

  board.gpio[0].feature = tef::GpioFeature::Davn;  // TEF GPIO_0 -> "RDS data waiting" (active low)
  board.gpio[0].mcuPin = 2;                        // wired to MCU pin 2 (TEF668XRds uses it)
  board.gpio[2].feature = tef::GpioFeature::AgcStep;  // external FM AGC step circuit; high in AM

  board.i2cHz = 400000;                        // bus speed; reads are timed for 400 kHz

  if (radio.begin(Wire, board) != tef::Error::Ok) {
    Serial.println(F("board description invalid"));
    while (true) {}
  }
  const tef::Error e = radio.init();
  Serial.print(F("init: "));
  Serial.println(static_cast<int>(e));         // 0 = Ok
  Serial.print(F("variant id: "));
  Serial.println(static_cast<int>(radio.variant()));

  // The AGC step wiring also needs the FM AGC extension switched on (your choice of start level):
  radio.setFmRfAgc(890, true);
}

void loop() {}
