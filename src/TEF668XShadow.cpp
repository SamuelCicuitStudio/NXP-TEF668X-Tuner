#include "TEF668XShadow.h"

#include <string.h>

void TEF668XShadow::record(tef::Cmd cmd, const int16_t* params, uint8_t count) {
  const tef::detail::CmdDesc d = tef::detail::readDesc(cmd);
  for (uint8_t i = 0; i < count && i < d.nMax; ++i) values_[d.param + i] = params[i];
  // Optional parameters not sent keep their previous value in the chip, so keep the larger count.
  uint8_t& n = count_[static_cast<uint8_t>(cmd)];
  if (count > n) n = count;
}

bool TEF668XShadow::get(tef::Cmd cmd, int16_t* out, uint8_t& count) const {
  const uint8_t n = count_[static_cast<uint8_t>(cmd)];
  if (n == 0 || out == nullptr) return false;
  const tef::detail::CmdDesc d = tef::detail::readDesc(cmd);
  for (uint8_t i = 0; i < n; ++i) out[i] = values_[d.param + i];
  count = n;
  return true;
}

void TEF668XShadow::clear() {
  memset(values_, 0, sizeof(values_));
  memset(count_, 0, sizeof(count_));
}
