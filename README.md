# TEF668X PlatformIO Library

Arduino/ESP library for the full TEF668X family:
- TEF6686
- TEF6687 (FMSI)
- TEF6688 (Digital Radio I/O)
- TEF6689 (FMSI + Digital Radio I/O)

Repository:
- `https://github.com/SamuelCicuitStudio/TEF668X_lib.git`

Author:
- `SamuelCircuitStudio`

## Board support

- Arduino AVR: Uno, Nano (ATmega328), Mega 2560
- ESP: ESP8266, ESP32
- Other Arduino-compatible targets with `Wire`/`TwoWire` support

Notes:
- Patch/LUT data is stored in flash on AVR (`PROGMEM`) so small AVR boards can be used.
- Mega 2560 is recommended if your sketch is large or uses many additional libraries.

## What is implemented

- I2C transport via `TwoWire`
- Startup flow:
  - stop/start
  - patch + LUT upload (`p2.24`)
  - reference clock setup
  - activate + state polling
- Variant handling:
  - auto-detect from `Get_Identification`
  - force variant policy if desired
  - runtime feature gates (`FMSI`, `Digital Radio`)
- TEF6686-ready FM profile:
  - `applyFmDefaultProfile()`
  - `applyFmCarProfile()`
- Tuning:
  - FM preset / FM search / release search mute
  - AM preset
- Audio:
  - volume (`0.1 dB`, signed)
  - mute
- Readback:
  - operation status
  - identification
  - FM quality
  - FM signal status
  - FM processing status
- Variant-gated APIs:
  - FMSI commands (`cmd 32`, `90..92`)
  - Digital radio commands (`cmd 30`, `83`, `84`, `135`)

## Variant selection (user-friendly)

You can either:
- auto-detect the installed variant (recommended), or
- force a specific variant policy.

```cpp
TEF668X radio;
radio.begin(Wire, 0x64);

// Option A: auto detect (default)
radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

// Option B: force if you know your hardware
// radio.setVariantPolicy(TEF668X::VariantPolicy::ForceTEF6689);
```

After init:

```cpp
auto detected = radio.getDetectedVariant();
auto active = radio.getActiveVariant();
bool mismatch = radio.hasVariantMismatch();
```

If a feature is not supported by the active variant, APIs return:
- `TEF668X::Error::UnsupportedFeature`

## Quick start (PlatformIO)

`platformio.ini`:

```ini
lib_deps =
  https://github.com/SamuelCicuitStudio/TEF668X_lib.git
```

Sketch:

```cpp
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Wire.begin();
  radio.begin(Wire, 0x64);
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

  if (radio.initAuto(9216000UL, false) != TEF668X::Error::Ok) return;

  // Works for all family devices as baseline FM setup
  radio.applyFmDefaultProfile(-80); // -8.0 dB
  radio.tuneFmPreset(10470);        // 104.70 MHz
  radio.setMute(false);

  // Variant-gated call examples
  if (radio.supportsFmsi()) {
    radio.setStereoImprovement(1);
  }
  if (radio.supportsDigitalRadio()) {
    radio.setDigitalRadio(1);
  }
}

void loop() {}
```

## Publishing to PlatformIO Registry

From this library folder:

```bash
pio pkg pack
pio pkg publish --owner <your-platformio-owner>
```

## Notes

- Patch data is large; target MCUs need enough flash.
- Patch chunk size is configurable via `setPatchChunkSize()`.
- Use low I2C speed for patch upload if needed:
  - `setI2CSpeeds(100000, 100000)`
