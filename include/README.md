# TEF668X.h API Guide

This file explains what is available in [`TEF668X.h`](./TEF668X.h) so users can quickly understand how to use the library API.

## What this header gives you

`TEF668X` is the main class for controlling NXP TEF668X radios over I2C (`TwoWire`).

Supported variants:
- TEF6686
- TEF6687 (adds FMSI features)
- TEF6688 (adds Digital Radio interface features)
- TEF6689 (FMSI + Digital Radio interface)

## Main feature groups

1. Initialization and startup
- `begin(...)`
- `initSoft(...)`
- `initHard(...)`
- `initAuto(...)`
- `setReferenceClock(...)`
- `isReady()`

2. Variant management
- `setVariantPolicy(...)`
- `getVariantPolicy()`
- `getDetectedVariant()`
- `getActiveVariant()`
- `hasVariantMismatch()`
- `supportsFmsi()`
- `supportsDigitalRadio()`

3. FM/AM tuning
- `tuneFmPreset(...)`
- `tuneFmSearch(...)`
- `tuneAmPreset(...)`
- `releaseSearchOrCheckMuteFmAm()`

4. Audio control
- `setVolumeDb10(...)`
- `setMute(...)`

5. Readback and monitoring
- `getOperationStatus(...)`
- `getIdentification(...)`
- `getQualityFm(...)`
- `getSignalStatusFm(...)`
- `getProcessingStatusFm(...)`
- `getInterfaceStatus(...)` (Digital Radio variants only)

6. Feature-specific controls
- FMSI:
  - `setStereoImprovement(...)`
  - `setStBandBlendTime(...)`
  - `setStBandBlendGain(...)`
  - `setStBandBlendBias(...)`
- Digital Radio:
  - `setDigitalRadio(...)`
  - `setDrBlend(...)`
  - `setDrOptions(...)`

7. High-level FM profile
- `applyFmDefaultProfile(...)`
- `applyFmCarProfile(...)`

8. Advanced low-level command APIs
- `writeWordsU16(...)`
- `writeWordsS16(...)`
- `readWords(...)`

Use these only if you need raw command access beyond the high-level API.

## Important enums and structs

- `TEF668X::Error`
  - API return status (`Ok`, `I2CError`, `Timeout`, `UnsupportedFeature`, etc.)
- `TEF668X::Variant`
  - Detected/active chip type (`TEF6686`, `TEF6687`, `TEF6688`, `TEF6689`)
- `TEF668X::VariantPolicy`
  - Auto detect or force a specific variant behavior map
- `TEF668X::OpState`
  - Runtime state (`Boot`, `Idle`, `ActiveStandby`, `ActiveFm`, `ActiveAm`)
- `TEF668X::Identification`
  - Device/HW/SW words from chip identification
- `TEF668X::QualityData`
  - FM quality metrics
- `TEF668X::ProcessingStatusFm`
  - FM processing percentages (softmute/highcut/stereo/st-hi blend)

## Units and value formats

- FM frequency: TEF units in 10 kHz steps
  - Example: `10170` = `101.70 MHz`
  - Valid in API: `6500..10800`
- AM frequency: kHz
  - Example: `999` = `999 kHz`
- Volume: `0.1 dB` signed
  - Example: `-80` = `-8.0 dB`
- Many quality fields are `x10` values:
  - `0.1 dBuV`, `0.1%`, `0.1 kHz`

## Recommended startup flow

```cpp
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Wire.begin();
  radio.begin(Wire, 0x64);

  // Optional tuning of bus behavior
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setPatchChunkSize(24);

  // Auto-detect variant (recommended)
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

  if (radio.initAuto(9216000UL, false) != TEF668X::Error::Ok) return;

  radio.applyFmDefaultProfile(-80);
  radio.tuneFmPreset(10170);
  radio.setMute(false);
}

void loop() {}
```

## Variant-aware usage

Always check feature support before calling variant-gated functions:

```cpp
if (radio.supportsFmsi()) {
  radio.setStereoImprovement(1);
}

if (radio.supportsDigitalRadio()) {
  radio.setDigitalRadio(1);
}
```

If a feature is not available on the active variant, APIs return:
- `TEF668X::Error::UnsupportedFeature`

## Error handling pattern

```cpp
TEF668X::Error e = radio.tuneFmPreset(10470);
if (e != TEF668X::Error::Ok) {
  // handle error
}
```

## Where to start

- For first bring-up: use `examples/basic_fm/basic_fm.ino`
- For variant check: use `examples/variant_probe/variant_probe.ino`
- For diagnostics: use `examples/fm_quality_monitor/fm_quality_monitor.ino`
