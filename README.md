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
  radio.setMute(false);                        // the chip starts muted
}
void loop() {}
```

Examples (`examples/`): `plug_and_go`, `board_config`, `rds_display`, `seek_scan`, `am_radio`, `custom_settings`, `deep_control`.

### Readable names

Every value has an uppercase name as well (`TEF668XNames.h`, included automatically). Both spellings are the
same typed value, so the compiler still checks them:

| Uppercase | Same as |
|---|---|
| `TEF_PRESET_NXP_FIELD_FM`, `TEF_PRESET_FM_CAR`, `TEF_PRESET_AM_DX` … | `tef::kPresetNxpFieldFM`, `tef::kPresetFmCar` … |
| `TEF_OK`, `TEF_ERR_OUT_OF_RANGE` … | `tef::Error::Ok`, `tef::Error::OutOfRange` … |
| `TEF_BAND_FM`, `TEF_REGION_EUROPE`, `TEF_TUNE_SEARCH`, `TEF_DEEMPHASIS_75US` | `tef::Band::FM`, `tef::Region::Europe` … |
| `TEF_CLOCK_12MHZ`, `TEF_ADDRESS_LOW`, `TEF_POWER_ENABLE_PIN`, `TEF_GPIO_RDS_DAVN` | board description values |
| `TEF_WSH_DUAL`, `TEF_SOFTMUTE`, `TEF_HIGHCUT_FIR`, `TEF_STEREO_FORCED_MONO` | weak-signal options |
| `TEF_RDS_DECODER`, `TEF_RDS_NORMAL`, `TEF_RDS_STRICT` … | RDS mode and policies |

### Volume

`radio.setVolumePercent(0…100)`: 0 % is silent, 100 % is 0 dB (no gain), in 0.5 dB steps that sound even.
`radio.setVolume(-599…240)` gives the exact value in 0.1 dB, including a boost up to +24 dB.

---

## How the library is designed

### Four levels of access

| Level | Use it for | Example |
|---|---|---|
| **L3 presets** | a working radio in one line | `radio.applyPreset(tef::kPresetFmCar);` |
| **L2 helpers** | change a feature as a whole, read values in units | `radio.setWsh(tef::WshFunction::Highcut, tef::Band::FM, cfg);` · `q.signalPercent()` |
| **L1 typed commands** | any setting, all parameters, named options | `radio.setStereoMin(tef::StereoMin::ForcedMono, 400);` |
| **L0 raw** | experiments, future commands | `radio.write(tef::Cmd::FmSoftmuteMax, p, 2);` · `radio.writeWords(...)` |

All levels end in the same checked `write()`: one table, generated from the NXP user manual, holds for each command
its module, number, parameter count, ranges, signed values, chip defaults and the variants that support it.
A wrong value returns `tef::Error::OutOfRange` (and `lastErrorParam()` says which parameter) and **nothing is sent**.

### Decisions (fixed for this library)

| Topic | Decision |
|---|---|
| **Automatic settings** | **None.** `begin()` + `init()` only start the chip (patch, clock, activate, GPIO wiring from your board description). The chip then runs on its own defaults and is **muted**. Every radio setting is your choice: a preset, a helper or a typed command. |
| **Board / hardware** | Described once in `tef::BoardConfig`: crystal or external clock, I²C address, FM and/or AM used, how the tuner is powered, what each TEF GPIO is wired to. The library adapts (see below). |
| **RDS error policy** | You choose: `Strict`, `Normal` (default), `Fast`, `Dx` or `Custom` (your own numbers). |
| **Config cache** | **Off by default.** Optional `TEF668XShadow` object (~580 bytes RAM) records every successful write. |
| **Names** | New API only (version 0.2.0 is a redesign; 0.1.x names are gone). |
| **Constants** | Every option is a typed `enum class`; every range and default is a `constexpr` (`tef::FmSoftmuteLevel::kStartDefault`). No magic numbers in user code, zero run-time cost. |
| **Memory** | No heap, no `String`, no `float`, no virtual functions. Tables and presets live in flash (PROGMEM on AVR and ESP8266). Unused functions cost no flash; optional modules cost no RAM unless created. |
| **Timing rules from NXP** | Built in: 50 µs read setup (works at 400 kHz), patch split to the Wire buffer, 5 ms I²C silence after tuner power-up, AM→FM AGC timing window. |

### Board description

```cpp
tef::BoardConfig board;
board.clockHz = tef::kClock12MHz;                  // kClock4MHz / kClock9216kHz (default) / kClock12MHz
board.externalClock = false;                       // true with kClock55466kHz (external clock input)
board.i2cAddress = tef::kI2cAddressLow;            // 0x64 (GPIO_2 low) or 0x65 (GPIO_2 high)
board.fm = true;  board.am = false;                // AM calls now return Error::BandDisabled
board.power = tef::TunerPower::EnablePin;          // SharedWithMcu (default) / Switched / EnablePin
board.powerPin = 7;  board.powerActiveHigh = true;
board.gpio[0].feature = tef::GpioFeature::Davn;    // TEF GPIO_0 = "RDS data waiting" ...
board.gpio[0].mcuPin = 2;                          // ... wired to MCU pin 2
board.gpio[2].feature = tef::GpioFeature::AgcStep; // external FM AGC step; driven high in AM automatically
board.i2cHz = 400000;                              // default 100 kHz
radio.begin(Wire, board);
```

| Field | What the library does with it |
|---|---|
| `clockHz`, `externalClock` | sent with Set_ReferenceClock at init; invalid combinations are refused by `begin()` |
| `i2cAddress` | bus address |
| `fm`, `am` | commands, tunes and presets for a disabled band return `BandDisabled` |
| `power`, `powerPin`, `powerActiveHigh`, `powerUpDelayMs` | `EnablePin`: switches the tuner on; `Switched`/`EnablePin`: waits before the first I²C transfer (NXP AN11254 2.8) |
| `gpio[n].feature` | programmed at init for FM and AM (AgcStep becomes a fixed high level in AM, RDS features are off in AM) |
| `gpio[n].mcuPin` | input used by `TEF668XRds` (Davn) to read the chip only when data waits |
| `i2cHz`, `i2cPatchHz`, `patchChunkBytes` | bus speeds; chunk is limited to the Wire buffer automatically |

### Start-up

`radio.init()` (mode `Auto`) reads the device state and does only what is needed:

| State found | Action |
|---|---|
| boot (after power-on) | upload the p2.24 patch + LUT → Start → idle → reference clock → Activate |
| idle | reference clock → Activate |
| active (e.g. MCU reset only) | keep running, settings are kept |

Then it identifies the variant and programs the GPIOs. `InitMode::Patch` forces a new patch, `InitMode::NoPatch`
refuses a chip that still needs it. `radio.checkAlive()` detects a power dip (the chip falls back to boot state and
loses its settings): call `init()` and re-apply your settings.

---

## Using it

### Tuning

```cpp
radio.tuneFM(10470);                        // 104.70 MHz, Preset (short mute)
radio.tuneFM(9800, tef::TuneMode::Search);  // tune muted, judge quality, then tuneEnd() or Preset
radio.tuneFM(9930, tef::TuneMode::AfUpdate);// inaudible check of an alternative frequency
radio.tuneAM(999);                          // MW 999 kHz (LW 144-288, MW 522-1710, SW 2300-27000)
radio.tuneEnd();                            // release the mute of Search / Check
radio.applyRegion(tef::Region::USA);        // 75 us de-emphasis; tef::bandPlan(Region) gives limits/steps
```

### Reading the reception

```cpp
tef::Quality q;
radio.readQuality(q);
q.ready();          // tuning finished
q.rssi();           // 0.1 dBuV
q.noise;            // FM ultrasonic noise / AM noise, 0.1 %
q.multipath();      // FM multipath, 0.1 %
q.offset;           // 0.1 kHz
q.signalPercent();  // 0...100 (level, reduced by noise and multipath)
q.coChannel();      // AM co-channel detected

tef::SignalStatus s;      radio.readSignal(s);       // s.stereo, s.digital
tef::ProcessingStatus ps; radio.readProcessing(ps);  // how much SoftMute / HighCut / mono is applied now
tef::AgcStatus agc;       radio.readAgc(agc);
```

### Weak-signal handling (the "sound" of the radio)

SoftMute, HighCut, Stereo blend and Stereo high blend use one model: detectors (RF level, ultrasonic noise,
multipath, modulation) start the control at `start` and reach 100 % over `slope`; timers set how fast it follows;
`max`/`min` limit the action. Change a whole function at once:

```cpp
tef::WshConfig cfg;
radio.wshDefaults(tef::WshFunction::Softmute, tef::Band::FM, cfg);  // chip defaults
cfg.level.start = 120;                                              // start at 12 dBuV
cfg.max.limit = 150;                                                // at most 15 dB
radio.setWsh(tef::WshFunction::Softmute, tef::Band::FM, cfg, tef::kWshLevel | tef::kWshMax);
```

Or one command: `radio.setSoftmuteLevel(tef::Band::FM, tef::WshMode::Dual, 120, 220);`.
`radio.restoreChipDefaults(tef::Band::FM)` writes back every documented default.

### Presets

| Preset | Source | Content |
|---|---|---|
| `kPresetNxpFieldFM` | NXP user manual "improved field performance" notes | multipath suppression + channel EQ on, faster SoftMute, HighCut / Stereo / StHiBlend tuned for noise and multipath, HighCut 2.4 kHz, LowCut 100 Hz |
| `kPresetNxpFieldAmMwSw` | NXP user manual | AM HighCut from 47 dBuV |
| `kPresetNxpFieldAmLw` | NXP user manual | LW HighCut 52 dBuV, SoftMute 34 dBuV / 30 dB, max 33 dB |
| `kPresetFmCar` | tested car sketch | complete FM car setup (NXP values + AGC 89 dBuV, noise blanker, RDS decoder …) |
| `kPresetFmDx`, `kPresetAmDx` | community practice | less SoftMute, narrow AM filter |
| `kPresetAmActiveAntenna` | NXP AN11254 2.2 | 18 dB antenna attenuation, -7 dB level offset |

Presets are layered (`applyPreset(base); applyPreset(mine);`). Write your own in the same format:

```cpp
const uint8_t myPreset[] TEF668X_PROGMEM = {
  TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(120),
  TEF_CMD(FmDeemphasis, 1), TEF_W(tef::Deemphasis::Us75),
  TEF_END
};
```

### RDS (optional module, ~170 bytes RAM)

```cpp
TEF668XRds rds(radio);
rds.setPolicy(tef::RdsPolicy::Normal);
void loop() {
  rds.update();                                   // at least every 80 ms
  if (rds.psChanged()) Serial.println(rds.ps());  // station name
  if (rds.rtChanged() && rds.rtComplete()) Serial.println(rds.rt());
}
```

| Policy | Accepted blocks | PS confirmations | RT confirmations | Use |
|---|---|---|---|---|
| `Strict` | error-free only | 3 | 3 | clean display, slower |
| `Normal` (default) | + small corrected errors | 2 | 2 | everyday |
| `Fast` | + large corrected errors | 1 | 1 | fastest, may flicker |
| `Dx` | + large corrected errors | 2 | 2 | weak stations |
| `Custom` | `setCustomPolicy({maxError, psConfirm, rtConfirm})` | | | your choice |

Decoded: `pi()`, `pty()`, `tp()`, `ta()`, `music()`, `ps()`, `rt()`, `af(i)` / `afCount()`, `clock()`; status:
`synced()`, `dataLost()`, `lastErrorLevel()`, `quality()`. Data is cleared automatically after a retune.

### Seek (optional module, ~20 bytes RAM)

```cpp
TEF668XSeek seek(radio);
seek.setPlan(tef::bandPlan(tef::Region::Europe));
seek.start(true);                                         // up
// in loop(): Found / NotFound / Failed is returned once
if (seek.update() == TEF668XSeek::Status::Found) Serial.println(seek.frequency());
```

Thresholds per band (`tef::SeekThresholds`: level, noise, multipath, offset, settle time).

### Settings copy (optional module, ~580 bytes RAM)

```cpp
TEF668XShadow shadow;
radio.attachShadow(&shadow);
int16_t p[7]; uint8_t n;
shadow.get(tef::Cmd::FmSoftmuteMax, p, n);   // last values written
```

### Errors

Every call returns `tef::Error`: `Ok`, `NotBegun`, `NotReady`, `I2CError`, `Timeout`, `InvalidArg`, `OutOfRange`
(`lastErrorParam()`), `ParamCount`, `UnsupportedFeature` (FMSI / digital radio on a variant without it),
`BandDisabled`, `BadState`, `PatchDataError`.

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
