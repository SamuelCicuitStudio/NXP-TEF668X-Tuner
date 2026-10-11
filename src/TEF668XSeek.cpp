// Non-blocking seek built on muted Search tunes and Get_Quality.
#include "TEF668XSeek.h"

#include <Arduino.h>

#include "TEF668X.h"

namespace {
constexpr uint16_t kStepTimeoutMs = 60;  // give up on a step if the quality never becomes ready
}

TEF668XSeek::TEF668XSeek(TEF668X& radio) : radio_(radio), plan_(tef::bandPlan(tef::Region::Europe)) {}

void TEF668XSeek::setThresholds(tef::Band band, const tef::SeekThresholds& t) {
  if (band == tef::Band::FM) {
    fm_ = t;
  } else {
    am_ = t;
  }
}

uint16_t TEF668XSeek::next(uint16_t f) const {
  const bool fm = band_ == tef::Band::FM;
  const uint16_t lo = fm ? plan_.fmMin : plan_.amMin;
  const uint16_t hi = fm ? plan_.fmMax : plan_.amMax;
  const uint16_t step = fm ? plan_.fmStep : plan_.amStep;
  if (f < lo || f > hi) return up_ ? lo : hi;
  if (up_) return (f + step > hi) ? lo : static_cast<uint16_t>(f + step);
  return (f < lo + step) ? hi : static_cast<uint16_t>(f - step);
}

bool TEF668XSeek::isStation(const tef::Quality& q) const {
  const tef::SeekThresholds& t = band_ == tef::Band::FM ? fm_ : am_;
  const uint16_t off = static_cast<uint16_t>(q.offset < 0 ? -q.offset : q.offset);
  if (q.level < t.minLevel || off > t.maxOffset) return false;
  if (t.maxNoise != 0xFFFF && q.noise > t.maxNoise) return false;
  if (band_ == tef::Band::FM && t.maxMultipath != 0xFFFF && q.multipath() > t.maxMultipath) return false;
  return true;
}

tef::Error TEF668XSeek::searchAt(uint16_t f) {
  freq_ = f;
  t0_ = millis();
  return band_ == tef::Band::FM ? radio_.tuneFM(f, tef::TuneMode::Search) : radio_.tuneAM(f, tef::TuneMode::Search);
}

tef::Error TEF668XSeek::start(bool up) {
  if (!radio_.isReady()) return tef::Error::NotReady;
  band_ = radio_.band();
  up_ = up;
  start_ = radio_.frequency();
  if (start_ == 0) start_ = band_ == tef::Band::FM ? plan_.fmMin : plan_.amMin;
  const tef::Error e = searchAt(next(start_));
  status_ = e == tef::Error::Ok ? Status::Running : Status::Idle;  // the error is returned to the caller
  return e;
}

TEF668XSeek::Status TEF668XSeek::update() {
  if (status_ != Status::Running) return Status::Idle;

  tef::Quality q;
  if (radio_.readQuality(q) != tef::Error::Ok) return finish(Status::Failed);
  const tef::SeekThresholds& t = band_ == tef::Band::FM ? fm_ : am_;
  const bool settled = q.ready() && q.age() >= t.settle;
  if (!settled && millis() - t0_ < kStepTimeoutMs) return status_;  // still settling

  if (settled && isStation(q)) {
    const tef::Error e = band_ == tef::Band::FM ? radio_.tuneFM(freq_) : radio_.tuneAM(freq_);
    return finish(e == tef::Error::Ok ? Status::Found : Status::Failed);
  }

  const uint16_t n = next(freq_);
  if (n == start_ || (up_ ? (freq_ < start_ && n > start_) : (freq_ > start_ && n < start_))) {
    cancel();
    return Status::NotFound;
  }
  if (searchAt(n) != tef::Error::Ok) return finish(Status::Failed);
  return Status::Running;
}

TEF668XSeek::Status TEF668XSeek::finish(Status result) {
  status_ = Status::Idle;
  return result;
}

void TEF668XSeek::cancel() {
  if (status_ == Status::Running && start_ != 0) {
    if (band_ == tef::Band::FM) {
      radio_.tuneFM(start_);
    } else {
      radio_.tuneAM(start_);
    }
    freq_ = start_;
  }
  status_ = Status::Idle;
}
