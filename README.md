# TEF668X

Arduino / ESP32 / ESP8266 library for the NXP **TEF6686, TEF6687, TEF6688, TEF6689** ("Lithio") AM/FM radio tuners.

- **Every command of the chip** is available, checked against the NXP user manual before it is sent.
- **Plug and go** with presets, or **full control** down to raw I²C words; mix both freely.
- **Small**: a complete FM radio sketch uses about 480 bytes of RAM on an Arduino Nano.

Repository: https://github.com/SamuelCicuitStudio/TEF668X_lib · Author: SamuelCircuitStudio · License: MIT

---

## Quick start

```cpp
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Wire.begin();
  radio.begin(Wire);                           // default board: 9.216 MHz crystal, address 0x64
  radio.init();                                // load the NXP patch, start, activate
  radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);  // NXP recommended FM sound
  radio.tuneFM(10470);                         // 104.70 MHz
  radio.setVolumePercent(80);                  // 80 % (= -10 dB)
  radio.setMute(TEF_OFF);                      // the chip starts muted
}
void loop() {}
```

Examples (`examples/`): `plug_and_go`, `board_config`, `rds_display`, `seek_scan`, `am_radio`, `audio_ducking`, `custom_settings`, `deep_control`.

### Short names: no `tef::` in your sketch

Everything you pass to the library has a short uppercase name (`TEF668XNames.h`, `TEF668XDefaults.h`, both
included automatically). Each one is the same typed value as the `tef::` spelling, so the compiler still checks it;
both styles work and can be mixed.

| What | Short names |
|---|---|
| Types for variables | `TEFBoard`, `TEFQuality`, `TEFSignal`, `TEFProcessing`, `TEFAgc`, `TEFWshConfig`, `TEFRdsGroup`, `TEFLastWrite`, `TEFError`, `TEFSeekStatus`, `TEFSeekThresholds`, `TEFRdsPolicyConfig`, `TEFBandPlan`, `TEFCmd` |
| Results | `TEF_OK`, `TEF_ERR_OUT_OF_RANGE`, `TEF_ERR_BAND_DISABLED`, `TEF_ERR_I2C` … |
| On / off for every `enable` parameter | `TEF_ON`, `TEF_OFF` |
| Bands, regions, tuning | `TEF_FM`, `TEF_AM`, `TEF_REGION_EUROPE` / `_USA` / `_JAPAN` / `_OIRT`, `TEF_TUNE_PRESET` / `_SEARCH` / `_AF_UPDATE` / `_JUMP` / `_CHECK` / `_END`, `TEF_FM_MIN` … `TEF_SW_MAX`, `TEF_BAND_PLAN(region)` |
| Start-up and state | `TEF_INIT_AUTO` / `_PATCH` / `_NO_PATCH`, `TEF_VARIANT_6686` … `_6689`, `TEF_STATE_BOOT` … `TEF_STATE_AM` |
| Board | `TEF_CLOCK_4MHZ` / `_9216KHZ` / `_12MHZ` / `_55466KHZ`, `TEF_ADDRESS_LOW` / `_HIGH`, `TEF_POWER_SHARED` / `_SWITCHED` / `_ENABLE_PIN`, `TEF_GPIO_UNUSED` / `_INPUT` / `_LOW` / `_HIGH` / `_RDS_DAVN` / `_QSI` / `_QSI_DAVN` / `_RDDA` / `_RDCL` / `_AGC_STEP` |
| Presets | `TEF_PRESET_NXP_FIELD_FM`, `_NXP_FIELD_AM_MW_SW`, `_NXP_FIELD_AM_LW`, `_FM_CAR`, `_FM_DX`, `_AM_DX`, `_AM_ACTIVE_ANTENNA` |
| Weak-signal handling | `TEF_WSH_OFF` / `_FAST` / `_SLOW` / `_DUAL`, `TEF_SOFTMUTE`, `TEF_HIGHCUT`, `TEF_STEREO`, `TEF_STHIBLEND`, `TEF_WSH_PART_TIME` … `_ALL`, `TEF_HIGHCUT_IIR` / `_DEEMPHASIS` / `_FIR`, `TEF_STEREO_MIN_OFF` / `_LIMIT`, `TEF_STEREO_FORCED_MONO`, `TEF_DEEMPHASIS_OFF` / `_50US` / `_75US` |
| RDS | `TEF_RDS_OFF` / `_DECODER` / `_DEMODULATOR`, `TEF_RDS_PIN_NONE` / `_DAVN` / `_LEGACY`, `TEF_RESTART_NONE` / `_NOW` / `_AFTER_TUNE`, policies `TEF_RDS_STRICT` / `_NORMAL` / `_FAST` / `_DX` / `_CUSTOM` |
| Seek | `TEF_SEEK_IDLE` / `_RUNNING` / `_FOUND` / `_NOT_FOUND` / `_FAILED`, `TEF_SEEK_FM_DEFAULT`, `TEF_SEEK_AM_DEFAULT` |
| Audio | `TEF_SOURCE_RADIO` / `_ANALOG_RADIO` / `_I2S_IN` / `_AUDIO_PROCESSOR` / `_SINE`, `TEF_OUTPUT_DAC` / `_I2S`, `TEF_I2S_INPUT` / `_OUTPUT`, `TEF_I2S_MODE_*`, `TEF_I2S_FORMAT_*`, `TEF_I2S_RATE_32K` / `_44K1` / `_48K`, `TEF_WAVE_*`, `TEF_SPECIAL_NORMAL` / `_MPX` / `_DIGITAL_ANALOG`, `TEF_QSI_PIN_NONE` / `_GPIO` |
| Digital radio | `TEF_DR_BLEND_PIN` / `_PIN_INVERTED` / `_DIGITAL` / `_ANALOG`, `TEF_MODULE_FM` … (raw access) |
| Values in plain units | volume `TEF_VOLUME_MUTE` / `_MINUS_20DB` / `_MINUS_10DB` / `_0DB` / `_MAX`; AM bandwidth `TEF_AM_BW_3KHZ` / `_4KHZ` / `_6KHZ` / `_8KHZ`; FM bandwidth `TEF_FM_BW_56KHZ` / `_114KHZ` / `_236KHZ` / `_311KHZ`; AM antenna `TEF_ANTENNA_0DB` … `_36DB`; FM AGC `TEF_FM_AGC_84DBUV` / `_89DBUV` / `_92DBUV` |
| Chip default of every parameter (227 names) | `TEF_<COMMAND>_<PARAMETER>_DEFAULT`, e.g. `TEF_FM_SOFTMUTE_LEVEL_START_DEFAULT` (150 = 15.0 dBuV), `TEF_AM_HIGHCUT_MAX_LIMIT_DEFAULT`, `TEF_FM_DEEMPHASIS_TIME_CONSTANT_DEFAULT` |

```cpp
TEFBoard board;
board.clockHz = TEF_CLOCK_12MHZ;            // TEF_CLOCK_4MHZ / TEF_CLOCK_9216KHZ (default) / TEF_CLOCK_12MHZ
board.externalClock = TEF_OFF;              // TEF_ON with TEF_CLOCK_55466KHZ (external clock)
board.i2cAddress = TEF_ADDRESS_LOW;         // TEF_ADDRESS_LOW (0x64) / TEF_ADDRESS_HIGH (0x65)
board.fm = TEF_ON;                          // bands used by the product
board.am = TEF_OFF;                         //   AM calls now return TEF_ERR_BAND_DISABLED
board.power = TEF_POWER_ENABLE_PIN;         // TEF_POWER_SHARED (default) / _SWITCHED / _ENABLE_PIN
board.powerPin = 7;
board.powerActiveHigh = TEF_ON;
board.gpio[0].feature = TEF_GPIO_RDS_DAVN;  // TEF GPIO_0 = "RDS data waiting" ...
board.gpio[0].mcuPin = 2;                   // ... wired to MCU pin 2
board.gpio[2].feature = TEF_GPIO_AGC_STEP;  // external FM AGC step; driven high in AM automatically
board.i2cHz = 400000;                       // default 100 kHz
radio.begin(Wire, board);
```

| Field | What the library does with it |
|---|---|
| `clockHz`, `externalClock` | sent with Set_ReferenceClock at init; invalid combinations are refused by `begin()` |
| `i2cAddress` | bus address |
| `fm`, `am` | commands, tunes and presets for a disabled band return `TEF_ERR_BAND_DISABLED` |
| `power`, `powerPin`, `powerActiveHigh`, `powerUpDelayMs` | `TEF_POWER_ENABLE_PIN`: switches the tuner on; `_SWITCHED` / `_ENABLE_PIN`: waits before the first I²C transfer (NXP AN11254 2.8) |
| `gpio[n].feature` | programmed at init for FM and AM (AgcStep becomes a fixed high level in AM, RDS features are off in AM) |
| `gpio[n].mcuPin` | input used by `TEF668XRds` (Davn) to read the chip only when data waits |
| `i2cHz`, `i2cPatchHz`, `patchChunkBytes` | bus speeds; chunk is limited to the Wire buffer automatically |

### Start-up

`radio.init()` (`TEF_INIT_AUTO`) reads the device state and does only what is needed:

| State found | Action |
|---|---|
| boot (after power-on) | upload the p2.24 patch + LUT → Start → idle → reference clock → Activate |
| idle | reference clock → Activate |
| active (e.g. MCU reset only) | keep running, settings are kept |

Then it identifies the variant and programs the GPIOs. `radio.init(TEF_INIT_PATCH)` forces a new patch, `TEF_INIT_NO_PATCH`
refuses a chip that still needs it. `radio.checkAlive()` detects a power dip (the chip falls back to boot state and
loses its settings): call `init()` and re-apply your settings.

---

## Using it

### Tuning

```cpp
radio.tuneFM(10470);                        // 104.70 MHz, Preset (short mute)
radio.tuneFM(9800, TEF_TUNE_SEARCH);  // tune muted, judge quality, then tuneEnd() or Preset
radio.tuneFM(9930, TEF_TUNE_AF_UPDATE);// inaudible check of an alternative frequency
radio.tuneAM(999);                          // MW 999 kHz (LW 144-288, MW 522-1710, SW 2300-27000)
radio.tuneEnd();                            // release the mute of Search / Check
radio.applyRegion(TEF_REGION_USA);        // 75 us de-emphasis; TEF_BAND_PLAN(region) gives limits/steps
```

### Reading the reception

```cpp
TEFQuality q;
radio.readQuality(q);
q.ready();          // tuning finished
q.rssi();           // 0.1 dBuV
q.noise;            // FM ultrasonic noise / AM noise, 0.1 %
q.multipath();      // FM multipath, 0.1 %
q.offset;           // 0.1 kHz
q.signalPercent();  // 0...100 (level, reduced by noise and multipath)
q.coChannel();      // AM co-channel detected

TEFSignal s;      radio.readSignal(s);       // s.stereo, s.digital
TEFProcessing ps; radio.readProcessing(ps);  // how much SoftMute / HighCut / mono is applied now
TEFAgc agc;       radio.readAgc(agc);
```

### Weak-signal handling (the "sound" of the radio)

SoftMute, HighCut, Stereo blend and Stereo high blend use one model: detectors (RF level, ultrasonic noise,
multipath, modulation) start the control at `start` and reach 100 % over `slope`; timers set how fast it follows;
`max`/`min` limit the action. Change a whole function at once:

```cpp
TEFWshConfig cfg;
radio.wshDefaults(TEF_SOFTMUTE, TEF_FM, cfg);  // chip defaults
cfg.level.start = 120;                                              // start at 12 dBuV
cfg.max.limit = 150;                                                // at most 15 dB
radio.setWsh(TEF_SOFTMUTE, TEF_FM, cfg, TEF_WSH_PART_LEVEL | TEF_WSH_PART_MAX);
```

Or one command: `radio.setSoftmuteLevel(TEF_FM, TEF_WSH_DUAL, 120, 220);`.
`radio.restoreChipDefaults(TEF_FM)` writes back every documented default.

### Presets

| Preset | Source | Content |
|---|---|---|
| `TEF_PRESET_NXP_FIELD_FM` | NXP user manual "improved field performance" notes | multipath suppression + channel EQ on, faster SoftMute, HighCut / Stereo / StHiBlend tuned for noise and multipath, HighCut 2.4 kHz, LowCut 100 Hz |
| `TEF_PRESET_NXP_FIELD_AM_MW_SW` | NXP user manual | AM HighCut from 47 dBuV |
| `TEF_PRESET_NXP_FIELD_AM_LW` | NXP user manual | LW HighCut 52 dBuV, SoftMute 34 dBuV / 30 dB, max 33 dB |
| `TEF_PRESET_FM_CAR` | tested car sketch | complete FM car setup (NXP values + AGC 89 dBuV, noise blanker, RDS decoder …) |
| `TEF_PRESET_FM_DX`, `TEF_PRESET_AM_DX` | community practice | less SoftMute, narrow AM filter |
| `TEF_PRESET_AM_ACTIVE_ANTENNA` | NXP AN11254 2.2 | 18 dB antenna attenuation, -7 dB level offset |

Presets are layered (`applyPreset(base); applyPreset(mine);`). Write your own in the same format:

```cpp
const uint8_t myPreset[] TEF668X_PROGMEM = {
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(120),
  TEF_CMD(FmDeemphasis, 1), TEF_W(TEF_DEEMPHASIS_75US),
  TEF_END
};
```

### RDS (optional module, ~170 bytes RAM)

```cpp
TEF668XRds rds(radio);
rds.setPolicy(TEF_RDS_NORMAL);
void loop() {
  rds.update();                                   // at least every 80 ms
  if (rds.psChanged()) Serial.println(rds.ps());  // station name
  if (rds.rtChanged() && rds.rtComplete()) Serial.println(rds.rt());
}
```

| Policy | Accepted blocks | PS confirmations | RT confirmations | Use |
|---|---|---|---|---|
| `TEF_RDS_STRICT` | error-free only | 3 | 3 | clean display, slower |
| `TEF_RDS_NORMAL` (default) | + small corrected errors | 2 | 2 | everyday |
| `TEF_RDS_FAST` | + large corrected errors | 1 | 1 | fastest, may flicker |
| `TEF_RDS_DX` | + large corrected errors | 2 | 2 | weak stations |
| `TEF_RDS_CUSTOM` | `setCustomPolicy({maxError, psConfirm, rtConfirm})` | | | your choice |

Decoded: `pi()`, `pty()`, `tp()`, `ta()`, `music()`, `ps()`, `rt()`, `af(i)` / `afCount()`, `clock()`; status:
`synced()`, `dataLost()`, `lastErrorLevel()`, `quality()`. Data is cleared automatically after a retune.

### Seek (optional module, ~20 bytes RAM)

```cpp
TEF668XSeek seek(radio);
seek.setPlan(TEF_BAND_PLAN(TEF_REGION_EUROPE));
seek.start(true);                                         // up
// in loop(): Found / NotFound / Failed is returned once
if (seek.update() == TEF_SEEK_FOUND) Serial.println(seek.frequency());
```

Thresholds per band (`TEFSeekThresholds`: level, noise, multipath, offset, settle time).

### Settings copy (optional module, ~580 bytes RAM)

```cpp
TEF668XShadow shadow;
radio.attachShadow(&shadow);
int16_t p[7]; uint8_t n;
shadow.get(TEFCmd::FmSoftmuteMax, p, n);   // last values written
```

### Errors

Every call returns a `TEFError`: `TEF_OK`, `TEF_ERR_NOT_BEGUN`, `TEF_ERR_NOT_READY`, `TEF_ERR_I2C`, `TEF_ERR_TIMEOUT`,
`TEF_ERR_INVALID_ARG`, `TEF_ERR_OUT_OF_RANGE` (`lastErrorParam()` says which parameter), `TEF_ERR_PARAM_COUNT`,
`TEF_ERR_UNSUPPORTED` (FMSI / digital radio on a variant without it), `TEF_ERR_BAND_DISABLED`, `TEF_ERR_BAD_STATE`,
`TEF_ERR_PATCH_DATA`.

### Variants

| | TEF6686 | TEF6687 | TEF6688 | TEF6689 |
|---|---|---|---|---|
| FM / AM / RDS / all weak-signal features | ✔ | ✔ | ✔ | ✔ |
| FMSI (`setStereoImprovement`, `setStBandBlend*`) | | ✔ | | ✔ |
| Digital radio (`setDigitalRadio`, `setDrBlend`, `setDrOptions`, `readDrSampleRate`) | | | ✔ | ✔ |

`radio.variant()` is read at init; variant-only commands return `UnsupportedFeature` on other chips.

---

## Reference

- **[docs/COMMANDS.md](docs/COMMANDS.md)**: every command, its function, parameters, ranges and chip defaults (generated).
- Header comments: each function documents what it does, its parameters with range and default, and the
  user-manual section.
- `patches/official/`: the original NXP p2.24 patch files.

## Memory (Arduino Nano, measured)

| Sketch | RAM | Flash |
|---|---|---|
| `plug_and_go` (FM + preset + quality) | 478 B | 15.8 KB |
| `rds_display` (+ RDS module) | 651 B | 17.4 KB |
| `seek_scan` (+ seek module) | 569 B | 16.5 KB |
| `custom_settings` (+ shadow module) | 1054 B | 16.8 KB |

About 6.2 KB of flash is the NXP patch, which every TEF668X /V102 needs.

## Generated files

`src/TEF668XCommands.h`, `src/TEF668XCommandMethods.inc`, `src/TEF668XCommandTable.cpp` and `docs/COMMANDS.md`
are generated from one command catalogue taken from the NXP user manual. Do not edit them by hand.

Sources: NXP UM TEF668X /V102 Rev 1.9, TEF668X datasheet Rev 6, AN11254 Rev 1.13, patch p2.24.
