#pragma once
/**
 * @file TEF668XSeek.h
 * @brief Optional non-blocking seek (about 20 bytes of RAM, only when created).
 *
 * How it works: each step is a muted "Search" tune (user manual 3.1). update() waits
 * until the quality data has settled, judges it against the thresholds, then either
 * stops with an audible "Preset" tune (station found) or steps on. The band plan
 * gives limits and step; the search wraps around and stops after one full turn.
 *
 * @code
 * TEF668XSeek seek(radio);
 * seek.setPlan(tef::bandPlan(tef::Region::Europe));
 * seek.start(true);                                  // up
 * TEF668XSeek::Status s;
 * while ((s = seek.update()) == TEF668XSeek::Status::Running) { }  // or poll in loop()
 * if (s == TEF668XSeek::Status::Found) { ... seek.frequency() ... }
 * @endcode
 */
#include <stdint.h>

#include "TEF668XTypes.h"

class TEF668X;

namespace tef {
/// When a frequency counts as a station.
struct SeekThresholds {
  int16_t minLevel;       ///< RF level, 0.1 dBuV
  uint16_t maxNoise;      ///< FM USN / AM noise, 0.1 % (0xFFFF = ignore)
  uint16_t maxMultipath;  ///< FM WAM, 0.1 % (ignored on AM)
  uint16_t maxOffset;     ///< |frequency offset|, 0.1 kHz
  uint16_t settle;        ///< quality age to wait for, 0.1 ms (max 320; 1000 = steady)
};
/// FM default: 20 dBuV, USN 25 %, WAM 25 %, offset 10 kHz, 10 ms settle.
constexpr SeekThresholds kSeekFmDefault{200, 250, 250, 100, 100};
/// AM default: 30 dBuV, noise ignored, offset 1.5 kHz, 20 ms settle.
constexpr SeekThresholds kSeekAmDefault{300, 0xFFFF, 0xFFFF, 15, 200};
}  // namespace tef

class TEF668XSeek {
 public:
  enum class Status : uint8_t {
    Idle,      ///< nothing running
    Running,   ///< stepping
    Found,     ///< stopped on a station (radio unmuted on it, frequency())
    NotFound,  ///< full turn without a station (back on the start frequency)
    Failed     ///< a tune or read failed
  };

  explicit TEF668XSeek(TEF668X& radio);

  /// Band limits and steps (default: Europe).
  void setPlan(const tef::BandPlan& plan) { plan_ = plan; }
  /// Station criteria for a band.
  void setThresholds(tef::Band band, const tef::SeekThresholds& t);

  /// Start from the current frequency of the current band. up = higher frequencies.
  tef::Error start(bool up);
  /**
   * @brief Advance the seek; call repeatedly. Never blocks.
   * @return Running while stepping; Found / NotFound / Failed exactly once at the end
   *         (the seek is then Idle again); Idle when nothing runs.
   */
  Status update();
  /// Stop and return to the start frequency.
  void cancel();

  /// Running or Idle.
  Status status() const { return status_; }
  /// Frequency being tested (or found).
  uint16_t frequency() const { return freq_; }

 private:
  uint16_t next(uint16_t f) const;
  bool isStation(const tef::Quality& q) const;
  tef::Error searchAt(uint16_t f);
  Status finish(Status result);

  TEF668X& radio_;
  tef::BandPlan plan_;
  tef::SeekThresholds fm_ = tef::kSeekFmDefault;
  tef::SeekThresholds am_ = tef::kSeekAmDefault;
  tef::Band band_ = tef::Band::FM;
  Status status_ = Status::Idle;
  bool up_ = true;
  uint16_t start_ = 0, freq_ = 0;
  uint32_t t0_ = 0;
};
