# TEF668X.h API Guide

This document explains what each public function in [`TEF668X.h`](./TEF668X.h) does.

`TEF668X` controls NXP TEF668X radios over I2C (`TwoWire`) and supports:
- TEF6686
- TEF6687 (FMSI features)
- TEF6688 (Digital Radio features)
- TEF6689 (FMSI + Digital Radio features)

## Important types

- `TEF668X::Error`: API return status (`Ok`, `I2CError`, `Timeout`, `UnsupportedFeature`, etc.).
- `TEF668X::Variant`: Detected/active chip variant.
- `TEF668X::VariantPolicy`: Auto-detect variant or force a chosen variant map.
- `TEF668X::OpState`: Boot/Idle/Active status values from the chip.
- `TEF668X::Identification`: device/hardware/software version words.
- `TEF668X::QualityData`: FM quality metrics.
- `TEF668X::ProcessingStatusFm`: FM processing percentages.

## Units used in APIs

- FM frequency uses TEF units in 10 kHz steps.
  - Example: `10170` means `101.70 MHz`.
- AM frequency uses kHz.
  - Example: `999` means `999 kHz`.
- Volume uses signed `0.1 dB`.
  - Example: `-80` means `-8.0 dB`.
- Several readback values are `x10` scaled (`0.1 dBuV`, `0.1%`, `0.1 kHz`).

## Function-by-function reference

### Lifecycle and setup

| Function | What it does | Returns |
|---|---|---|
| `TEF668X()` | Creates the controller object. Hardware access starts only after `begin()`. | Constructor |
| `begin(wire, i2cAddress)` | Binds the I2C bus and chip address (default `0x64`). Must be called first. | `bool` (`true` on success) |
| `setI2CSpeeds(patchHz, runHz)` | Sets I2C speed for patch upload stage and normal runtime stage. | `void` |
| `setPatchChunkSize(chunkBytes)` | Sets patch upload chunk size; library enforces even-byte boundary. | `void` |
| `setVariantPolicy(policy)` | Sets variant policy (`AutoDetect` or force one variant behavior map). | `void` |
| `getVariantPolicy()` | Returns current variant policy. | `VariantPolicy` |

### Initialization

| Function | What it does | Returns |
|---|---|---|
| `initSoft(crystalHz, externalClock, timeoutMs)` | Soft init path (no patch upload), sets reference clock and activates radio. | `Error` |
| `initHard(crystalHz, externalClock, timeoutMs)` | Hard init path with patch + LUT upload, then activate. | `Error` |
| `initAuto(crystalHz, externalClock)` | Tries soft init first, then hard init fallback. | `Error` |
| `isReady()` | Tells if initialization completed successfully and runtime is ready. | `bool` |
| `setReferenceClock(frequencyHz, externalClock)` | Writes reference clock setting (APPL cmd 4). Usually used in idle before activate. | `Error` |

### FM/AM profiles and tuning

| Function | What it does | Returns |
|---|---|---|
| `applyFmDefaultProfile(volume_x10_dB)` | Applies default FM profile alias (currently maps to car profile). | `Error` |
| `applyFmCarProfile(volume_x10_dB)` | Applies TEF6686-oriented FM car profile (bandwidth/AGC/weak-signal/stereo/RDS/scaler/volume). | `Error` |
| `tuneFmPreset(frequency_kHz)` | FM preset tune (`mode=1`). | `Error` |
| `tuneFmSearch(frequency_kHz)` | FM search tune (`mode=2`, muted until release). | `Error` |
| `tuneAmPreset(frequency_kHz)` | AM preset tune (`mode=1`). | `Error` |
| `releaseSearchOrCheckMuteFmAm()` | Releases mute after search/check action (`mode=7`) on current FM/AM module. | `Error` |

### Audio control

| Function | What it does | Returns |
|---|---|---|
| `setVolumeDb10(volume_x10_dB)` | Sets output volume in signed `0.1 dB`. | `Error` |
| `setMute(mute)` | Enables or disables mute. | `Error` |

### Readback and status

| Function | What it does | Returns |
|---|---|---|
| `getOperationStatus(status)` | Reads operation state (`Boot/Idle/Active...`). | `Error` |
| `getIdentification(out)` | Reads chip identification words (`device/hw/sw`). | `Error` |
| `getQualityFm(out)` | Reads FM quality dataset. | `Error` |
| `getSignalStatusFm(status)` | Reads FM signal status bitfield. | `Error` |
| `getProcessingStatusFm(out)` | Reads FM processing percentages (softmute/highcut/stereo/st-hi-blend). | `Error` |
| `getInterfaceStatus(sampleRate_kHz_x1)` | Reads Digital Radio interface status. Supported only on DR-capable variants. | `Error` |

### Variant and capability helpers

| Function | What it does | Returns |
|---|---|---|
| `getActiveVariant()` | Returns active variant used by feature checks (detected or forced). | `Variant` |
| `getDetectedVariant()` | Returns hardware-detected variant from identification. | `Variant` |
| `hasVariantMismatch()` | `true` when forced policy differs from detected hardware. | `bool` |
| `supportsFmsi()` | `true` when active variant supports FMSI APIs. | `bool` |
| `supportsDigitalRadio()` | `true` when active variant supports Digital Radio APIs. | `bool` |

### FMSI feature APIs (variant-gated)

| Function | What it does | Returns |
|---|---|---|
| `setStereoImprovement(mode)` | Enables/configures FMSI stereo improvement. | `Error` |
| `setStBandBlendTime(attack_ms, decay_ms)` | Sets FMSI band-blend timing. | `Error` |
| `setStBandBlendGain(band1, band2, band3, band4)` | Sets FMSI blend gains for four bands. | `Error` |
| `setStBandBlendBias(band1, band2, band3, band4)` | Sets FMSI blend bias for four bands. | `Error` |

### Digital Radio feature APIs (variant-gated)

| Function | What it does | Returns |
|---|---|---|
| `setDigitalRadio(mode)` | Enables/disables digital radio path. | `Error` |
| `setDrBlend(mode, inTime_ms, outTime_ms, gain_x10_dB)` | Configures analog/digital blend behavior. | `Error` |
| `setDrOptions(sampleRate, mode, format)` | Configures Digital Radio interface options. | `Error` |

### Advanced low-level command APIs

| Function | What it does | Returns |
|---|---|---|
| `writeWordsU16(module, cmd, index, words, wordCount)` | Sends raw unsigned word payload to a TEF command. | `Error` |
| `writeWordsS16(module, cmd, index, words, wordCount)` | Sends raw signed word payload to a TEF command. | `Error` |
| `readWords(module, cmd, index, out, wordCount)` | Reads raw words from a TEF command. | `Error` |

Use low-level APIs only when high-level methods do not cover your use case.

## Recommended startup flow

```cpp
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

void setup() {
  Wire.begin();
  radio.begin(Wire, 0x64);
  radio.setI2CSpeeds(100000UL, 100000UL);
  radio.setPatchChunkSize(24);
  radio.setVariantPolicy(TEF668X::VariantPolicy::AutoDetect);

  if (radio.initAuto(9216000UL, false) != TEF668X::Error::Ok) return;

  radio.applyFmDefaultProfile(-80);
  radio.tuneFmPreset(10170);
  radio.setMute(false);
}

void loop() {}
```

## Variant-gated behavior

Check capability first:

```cpp
if (radio.supportsFmsi()) {
  radio.setStereoImprovement(1);
}
if (radio.supportsDigitalRadio()) {
  radio.setDigitalRadio(1);
}
```

When not supported, these APIs return `TEF668X::Error::UnsupportedFeature`.

## Quick pointers

- First bring-up: `examples/basic_fm/basic_fm.ino`
- Full TEF6686 console flow: `examples/tef6686_full_console/tef6686_full_console.ino`
- Variant check: `examples/variant_probe/variant_probe.ino`
