#pragma once
/**
 * @file TEF668XRds.h
 * @brief Optional RDS / RBDS decoder (costs ~140 bytes of RAM only when you create it).
 *
 * The TEF668X demodulates RDS, corrects errors and buffers up to 22 groups; it marks
 * each block A..D with an error level (0 none, 1 small corrected, 2 large corrected,
 * 3 uncorrectable). This class empties that buffer, keeps only the blocks your
 * policy accepts, confirms text by repetition and decodes the services:
 * PI, PTY, TP, TA, music/speech, PS name, RadioText (A/B), AF list and clock time.
 *
 * @code
 * TEF668XRds rds(radio);                      // policy Normal by default
 * rds.setPolicy(TEF_RDS_STRICT);      // or Fast, Dx, Custom
 * void loop() {
 *   rds.update();                             // call at least every 80 ms
 *   if (rds.psChanged()) Serial.println(rds.ps());
 * }
 * @endcode
 *
 * The RDS decoder must be on (chip default: decoder mode, auto restart after tune).
 * If a TEF GPIO is wired as Davn/QsiDavn with an MCU pin in BoardConfig, update()
 * reads the chip only when that pin says data is waiting.
 */
#include <stdint.h>

#include "TEF668XTypes.h"

class TEF668X;

namespace tef {

/// How strict the decoder is with damaged data.
enum class RdsPolicy : uint8_t {
  Strict,  ///< error-free blocks only, text confirmed 3x: clean but slower
  Normal,  ///< accept small corrected errors, text confirmed 2x (default)
  Fast,    ///< accept corrected errors, show text at once: fastest, may flicker
  Dx,      ///< accept corrected errors but confirm text 2x: weak signals
  Custom   ///< values from setCustomPolicy()
};

/// Settings behind a policy.
struct RdsPolicyConfig {
  uint8_t maxError;   ///< highest accepted block error level (0, 1 or 2)
  uint8_t psConfirm;  ///< identical receptions needed before a PS character pair is shown
  uint8_t rtConfirm;  ///< identical receptions needed before a RadioText segment is shown
};

/// Clock time from group 4A (UTC + local offset).
struct RdsClock {
  bool valid = false;
  uint32_t mjd = 0;        ///< modified Julian day
  uint8_t hour = 0;        ///< UTC
  uint8_t minute = 0;
  int8_t offsetHalfHours = 0;  ///< local time = UTC + offset * 30 min
};

}  // namespace tef

class TEF668XRds {
 public:
  /// Bind to a radio. Nothing is read until update().
  explicit TEF668XRds(TEF668X& radio);

  /// Choose a policy (Strict / Normal / Fast / Dx). Custom uses setCustomPolicy().
  void setPolicy(tef::RdsPolicy policy);
  /// Define and select a custom policy.
  void setCustomPolicy(const tef::RdsPolicyConfig& config);
  tef::RdsPolicy policy() const { return policy_; }

  /**
   * @brief Read and decode every buffered group. Call at least every 80 ms (the chip
   *        keeps 22 groups, about 1.9 s, but reading often gives faster text).
   * @return number of groups read (0 = nothing new or not on FM)
   * @details Clears all data automatically when the radio was retuned.
   */
  uint8_t update();

  /// Forget everything (done automatically on a new frequency).
  void clear();

  // ---- state of the RDS reception
  bool synced() const { return synced_; }             ///< decoder locked on an RDS signal
  bool dataLost() const { return lost_; }             ///< groups were dropped (update() called too late)
  uint8_t lastErrorLevel() const { return lastErr_; } ///< worst block error of the last group (0..3)
  /// Share of received groups accepted by the policy since clear(), 0...100.
  uint8_t quality() const;

  // ---- decoded services
  bool hasPi() const { return piValid_; }
  uint16_t pi() const { return pi_; }                 ///< programme identification
  uint8_t pty() const { return pty_; }                ///< programme type 0...31
  bool tp() const { return tp_; }                     ///< traffic programme
  bool ta() const { return ta_; }                     ///< traffic announcement now
  bool music() const { return ms_; }                  ///< music (true) or speech
  const char* ps() const { return ps_; }              ///< station name, 8 chars, confirmed by the policy
  bool psChanged();                                   ///< true once after the name changed
  const char* rt() const { return rt_; }              ///< RadioText, up to 64 chars
  bool rtComplete() const;                            ///< every segment up to the end confirmed
  bool rtChanged();                                   ///< true once after the text changed
  uint8_t afCount() const { return afCount_; }        ///< alternative frequencies collected
  uint16_t af(uint8_t i) const;                       ///< AF i in 10 kHz (8760 = 87.6 MHz), 0 if none
  const tef::RdsClock& clock() const { return clock_; }

 private:
  void decode(const tef::RdsGroup& g);
  bool accept(const tef::RdsGroup& g, uint8_t block) const { return g.error(block) <= cfg_.maxError; }
  void psPair(uint8_t pos, char a, char b);
  void rtChars(uint8_t seg, const char* c, uint8_t n);
  void addAf(uint8_t code);

  TEF668X& radio_;
  tef::RdsPolicy policy_ = tef::RdsPolicy::Normal;
  tef::RdsPolicyConfig cfg_{1, 2, 2};
  uint16_t lastFreq_ = 0;

  bool synced_ = false, lost_ = false, piValid_ = false;
  bool tp_ = false, ta_ = false, ms_ = false;
  bool psNew_ = false, rtNew_ = false, rtAb_ = false;
  uint8_t lastErr_ = 0, pty_ = 0;
  uint16_t pi_ = 0;
  uint16_t groups_ = 0, accepted_ = 0;

  char ps_[9] = "        ";
  char psCand_[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  uint8_t psCount_[4] = {0};          // confirmations per character pair
  char rt_[65] = {0};
  uint8_t rtCount_[16] = {0};         // confirmations per segment
  int8_t rtEnd_ = -1;                 // segment holding the end mark (0x0D), -1 unknown
  uint8_t afCodes_[12] = {0};
  uint8_t afCount_ = 0;
  tef::RdsClock clock_;
};
