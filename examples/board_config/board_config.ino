/*
  board_config - describe YOUR hardware once; the library adapts to it.

  Every field of TEFBoard is optional; change only what differs from the
  defaults (9.216 MHz crystal, address 0x64, FM + AM, shared supply, no GPIO used).
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  TEFBoard board;
  board.clockHz = TEF_CLOCK_12MHZ;            // TEF_CLOCK_4MHZ / TEF_CLOCK_9216KHZ / TEF_CLOCK_12MHZ
  board.externalClock = TEF_OFF;               // TEF_ON + TEF_CLOCK_55466KHZ for an external clock
  board.i2cAddress = TEF_ADDRESS_LOW;      // TEF_ADDRESS_LOW (0x64) / TEF_ADDRESS_HIGH (0x65)
  board.fm = TEF_ON;                              // bands used by the product:
  board.am = TEF_OFF;                            //   AM commands now return TEF_ERR_BAND_DISABLED

  board.power = TEF_POWER_ENABLE_PIN;    // TEF_POWER_SHARED / TEF_POWER_SWITCHED / TEF_POWER_ENABLE_PIN
  board.powerPin = 7;                          // MCU pin that switches the tuner supply
  board.powerActiveHigh = TEF_ON;
  board.powerUpDelayMs = 10;                   // no I2C during the first 5 ms (AN11254 2.8)

  board.gpio[0].feature = TEF_GPIO_RDS_DAVN;  // TEF GPIO_0 -> "RDS data waiting" (active low)
  board.gpio[0].mcuPin = 2;                        // wired to MCU pin 2 (TEF668XRds uses it)
  board.gpio[2].feature = TEF_GPIO_AGC_STEP;  // external FM AGC step circuit; high in AM

  board.i2cHz = 400000;                        // bus speed; reads are timed for 400 kHz

  if (radio.begin(Wire, board) != TEF_OK) {
    Serial.println(F("board description invalid"));
    while (true) {}
  }
  const TEFError e = radio.init();
  Serial.print(F("init: "));
  Serial.println(static_cast<int>(e));         // 0 = Ok
  Serial.print(F("variant id: "));
  Serial.println(static_cast<int>(radio.variant()));

  // The AGC step wiring also needs the FM AGC extension switched on (your choice of start level):
  radio.setFmRfAgc(TEF_FM_AGC_89DBUV, TEF_ON);
}

void loop() {}
