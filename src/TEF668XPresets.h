#pragma once
/**
 * @file TEF668XPresets.h
 * @brief Ready-made settings (L3) and the format to write your own.
 *
 * A preset is a byte list in flash: for each entry the command id, the number of
 * parameters, then each parameter as a 16-bit word (MSB first); 0xFF ends the list.
 * TEF668X::applyPreset() sends every entry through the normal checked write(), so
 * a preset can never send an out-of-range value. Presets only cost flash when used.
 *
 * Presets are layered: apply a base, then a region, then your own adjustments.
 * Nothing is applied automatically.
 *
 * @code
 * // your own preset
 * const uint8_t myPreset[] TEF668X_PROGMEM = {
 *   TEF_CMD(FmSoftmuteMax, 2), TEF_W(1), TEF_W(150),     // 15 dB max SoftMute
 *   TEF_CMD(FmHighcutMax, 2),  TEF_W(1), TEF_W(3000),    // HighCut down to 3 kHz
 *   TEF_END
 * };
 * radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);
 * radio.applyPreset(myPreset);
 * @endcode
 */
#include <stdint.h>

#include "TEF668XCommands.h"

/// Start an entry: command name from tef::Cmd and its number of parameters.
#define TEF_CMD(name, count) static_cast<uint8_t>(tef::Cmd::name), static_cast<uint8_t>(count)
/// One parameter word (signed values allowed).
#define TEF_W(value) \
  static_cast<uint8_t>(static_cast<uint16_t>(static_cast<int16_t>(value)) >> 8), \
  static_cast<uint8_t>(static_cast<uint16_t>(static_cast<int16_t>(value)) & 0xFF)
/// End of the preset.
#define TEF_END 0xFF

namespace tef {

/**
 * NXP "suggested for improved field performance" FM values (user manual notes in
 * 3.7, 3.8, 3.18-3.21): multipath suppression + channel EQ on, faster SoftMute,
 * HighCut/Stereo/StHiBlend reacting earlier to noise and multipath, HighCut max
 * 2.4 kHz, LowCut max 100 Hz. Everything else stays at the chip default.
 */
extern const uint8_t kPresetNxpFieldFM[];

/// NXP suggested AM values for MW and SW: HighCut on level from 47 dBuV.
extern const uint8_t kPresetNxpFieldAmMwSw[];

/// NXP suggested AM values for LW: HighCut from 52 dBuV, SoftMute 34 dBuV/30 dB, max 33 dB.
extern const uint8_t kPresetNxpFieldAmLw[];

/**
 * Complete FM car setup from the client's tested TEF6686 sketch (tef6686done.ino):
 * the NXP field values plus RF AGC 89 dBuV, noise blanker, HighCut min 10 kHz,
 * LowCut min 20 Hz, Stereo level 46 dBuV, StHiBlend max 3 kHz, RDS decoder on.
 */
extern const uint8_t kPresetFmCar[];

/// FM long-distance listening (community practice): SoftMute max 6 dB, HighCut max 3 kHz.
extern const uint8_t kPresetFmDx[];

/// AM long-distance listening (community practice): 3 kHz bandwidth, SoftMute max 6 dB.
extern const uint8_t kPresetAmDx[];

/// AM with an active antenna (AN11254 2.2): 18 dB minimum attenuation, level offset -7 dB.
extern const uint8_t kPresetAmActiveAntenna[];

}  // namespace tef
