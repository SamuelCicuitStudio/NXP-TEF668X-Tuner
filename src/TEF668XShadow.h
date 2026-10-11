#pragma once
/**
 * @file TEF668XShadow.h
 * @brief Optional copy of the last value written to every command (~580 bytes of RAM).
 *
 * The TEF668X cannot read most settings back. Attach a shadow and every successful
 * write() (from L1, L2 or presets) is recorded, so the sketch can show or save the
 * current configuration. Off by default because of its RAM cost on small AVRs.
 *
 * @code
 * TEF668XShadow shadow;
 * radio.attachShadow(&shadow);
 * int16_t p[7]; uint8_t n;
 * if (shadow.get(TEFCmd::FmSoftmuteMax, p, n)) { ... }   // p[1] = limit last written
 * @endcode
 */
#include <stdint.h>

#include "TEF668XCommands.h"

class TEF668XShadow {
 public:
  /// Last parameters written to cmd. Returns false if it was never written.
  bool get(tef::Cmd cmd, int16_t* out, uint8_t& count) const;
  /// True if cmd was written since clear().
  bool written(tef::Cmd cmd) const { return count_[static_cast<uint8_t>(cmd)] != 0; }
  /// Forget everything (e.g. after a chip reset).
  void clear();
  /// Called by TEF668X::write() after a successful write.
  void record(tef::Cmd cmd, const int16_t* params, uint8_t count);

 private:
  int16_t values_[tef::detail::kParamTotal] = {0};
  uint8_t count_[static_cast<uint8_t>(tef::Cmd::Count)] = {0};
};
