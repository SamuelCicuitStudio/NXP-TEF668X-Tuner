/*
  TEF6686 FM "Car" firmware (Arduino Nano) - v7 (full manual audit)
  ------------------------------------------------------------------
  REBUILT against the TEF668X User Manual V102 + Patches.h p2.24.
  Every single FM / Audio / APPL command relevant to a TEF6686 in a
  car has been audited against the manual. See "WHAT CHANGED vs v6"
  below for the v7 additions.

  ================ WHAT CHANGED vs v6 ================

  [ADD] Set_ReferenceClock (APPL cmd 4, UM §3.40) — now called in
        idle state before Activate, as the manual requires. Crystal
        frequency is set via the TEF_CRYSTAL_HZ macro (default
        9216000 = 9.216 MHz which fits almost all PE5PVB-style
        breakouts). Change the macro if your module differs.

  [ADD] Set_RFAGC (FM cmd 11, UM §3.4) — RF front-end AGC start
        level set to 89 dBuV (UM application-example value) with no
        step extension. Lower start = earlier AGC = better intermod
        protection near high-power transmitters, which is exactly
        the situation in a car driving through an urban FM band.

  [ADD] Set_RDS (FM cmd 81, UM §3.23) — explicitly enabled in
        decoder mode with auto-restart-after-tune. Defaults already
        match this, but explicit call guarantees state across warm
        resets.

  [ADD] Full StHiBlend configuration (cmds 72–75). v6 only set the
        StHiBlend_Time; v7 also sets Level / Noise / Mph / Max.
        Noise and Mph use the UM "improved field performance" values
        (8% start instead of 16% default). StHiBlend narrows stereo
        channel separation on HIGH audio frequencies only when the
        signal is marginal — the single biggest improvement you can
        make for high-frequency multipath hiss without killing the
        stereo image on mid/low frequencies.

  [TUNE] Set_Stereo_Noise / Set_Stereo_Mph updated to UM field-perf
         values (§3.20 notes): 12%/16% and 10%/15% instead of the
         24%/20% defaults. Much more aggressive blend-to-mono on
         noise and multipath. Another multipath-cleanup improvement.

  ================ WHAT CHANGED vs v5 (from v6) ================

  [FIX 1] Set_Stereo_Max (cmd 65) now sends 1 parameter, not 2.
          The UM says cmd 65 takes ONLY `mode`. v5 passed wordCount=2
          with a 1-element array, so it was reading past the end of
          the stack and sending garbage as a phantom 2nd parameter.
          This corrupted stereo-blend behaviour.

  [FIX 2] Removed the bogus cmd 82 call that was labelled "Noise
          Blanker". Cmd 82 is actually Set_QualityStatus (UM §3.24);
          the real noise blanker is cmd 23 (already configured).

  [FIX 3] Removed the Set_SoftMute_Max {0,0} call. Softmute is
          exactly what you WANT in a car (it hides weak-signal hiss
          under bridges, fringes, tunnels). It is now explicitly set
          to mode=1, 20 dB max attenuation (FM default).

  [TUNE 1] FM bandwidth lowered from 311 kHz to 236 kHz (the manual
           default, 2360). Brussels FM is dense; 311 kHz was letting
           adjacent stations bleed in and pump the softmute/blend.
           --> Fixes the "below 100 MHz the sound cuts in and out".

  [TUNE 2] Noise blanker sensitivity reduced from 150% -> 100%.
           150% was aggressive and can cause audio "pumping" on
           strong stations with heavy modulation.

  [TUNE 3] All weak-signal curves (SoftMute/HighCut/Stereo) now use
           the manual's "improved field performance" values from
           UM §3.18 / §3.19 / §3.20. In particular:
             - Stereo Level start = 46 dBuV (was 30 dBuV): the chip
               now blends to mono MUCH earlier, which kills multipath
               noise on weak stations. This is the single biggest
               fix for "move an inch and the sound changes".
             - HighCut Noise/Mph use the UM field-perf values
               (15%/20% and 12%/16%) to cut treble on multipath
               before it becomes audible hiss.
             - HighCut Max limited to 2.4 kHz (UM field-perf) and
               HighCut Min at 10 kHz so audio stays intelligible
               all the way down to very weak signals.

  [TUNE 4] LowCut Min set to 20 Hz (protects the aux-in/speakers
           from DC/subsonic thump).

  [ROBUSTNESS] Init now POLLS APPL Get_Operation_Status (cmd 128)
               instead of using fixed delay()s, as recommended by
               UM §6.1. State targets: 1=idle, 2=active (standby),
               3=active FM. Fallback delays are still there for
               extra safety.

  Serial @115200:
    x / X        heartbeat keepalive
    f104.7       tune (104.7 MHz)
    f1047        tune (104.7 MHz)
    f104700      tune (104700 kHz)
    v-60         set volume attenuation in 0.1 dB steps (signed, -60=-6.0 dB)
    q            print one status block now
    r            reset RDS buffers
    reinit       SOFT init (no patch)
    hardinit     HARD init (patch)
*/

#include <Wire.h>
#include <avr/pgmspace.h>
#include "Patches.h"

#define TEF_I2C_ADDR  0x64
#define I2C_SPEED_PATCH 100000UL
#define I2C_SPEED_RUN   100000UL

#define DEBUG_SERIAL

// Crystal frequency on the TEF6686 module. Almost every PE5PVB-style
// TEF6686 breakout from AliExpress uses 9.216 MHz. If your module has
// a 4 MHz or 12 MHz crystal instead, change this to 4000000UL or
// 12000000UL. Set_ReferenceClock is called in idle state before Activate
// per UM §3.40.
#define TEF_CRYSTAL_HZ  9216000UL

#define DEFAULT_FREQ_KHZ   104700
#define DEFAULT_VOL_X01DB  (-80)   // -8.0 dB

// Heartbeat settings
#define HEARTBEAT_TIMEOUT_MS 4000UL
#define STATUS_PRINT_MS      5000UL

// ===================== I2C helpers =====================
static bool i2cWriteRawOK(const uint8_t* data, size_t len) {
  Wire.beginTransmission(TEF_I2C_ADDR);
  Wire.write(data, len);
  return (Wire.endTransmission() == 0);
}

static bool tefWriteWordsU16(uint8_t module, uint8_t cmd, uint8_t index, const uint16_t* words, uint8_t wordCount) {
  Wire.beginTransmission(TEF_I2C_ADDR);
  Wire.write(module); Wire.write(cmd); Wire.write(index);
  for (uint8_t i=0;i<wordCount;i++) {
    uint16_t w = words[i];
    Wire.write((uint8_t)(w >> 8));
    Wire.write((uint8_t)(w & 0xFF));
  }
  return (Wire.endTransmission() == 0);
}

static bool tefWriteWordsS16(uint8_t module, uint8_t cmd, uint8_t index, const int16_t* words, uint8_t wordCount) {
  Wire.beginTransmission(TEF_I2C_ADDR);
  Wire.write(module); Wire.write(cmd); Wire.write(index);
  for (uint8_t i=0;i<wordCount;i++) {
    uint16_t w = (uint16_t)words[i]; // two's complement
    Wire.write((uint8_t)(w >> 8));
    Wire.write((uint8_t)(w & 0xFF));
  }
  return (Wire.endTransmission() == 0);
}

static bool tefReadWords(uint8_t module, uint8_t cmd, uint8_t index, uint16_t* out, uint8_t wordCount) {
  Wire.beginTransmission(TEF_I2C_ADDR);
  Wire.write(module); Wire.write(cmd); Wire.write(index);
  if (Wire.endTransmission(false) != 0) return false;

  uint8_t bytes = (uint8_t)(wordCount * 2);
  uint8_t got = Wire.requestFrom((int)TEF_I2C_ADDR, (int)bytes);
  if (got != bytes) return false;

  for (uint8_t i=0;i<wordCount;i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    out[i] = (uint16_t)((hi << 8) | lo);
  }
  return true;
}

// ===================== Operation status polling (UM §6.1) =====================
// Reading APPL cmd 128 Get_Operation_Status is available in boot/idle/active.
// Returned values: 0=boot, 1=idle, 2=active standby, 3=active FM, 4=active AM.
static bool tefGetOperationStatus(uint16_t &status) {
  return tefReadWords(0x40, 0x80, 0x01, &status, 1);
}

static bool tefWaitForState(uint16_t target, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < timeout_ms) {
    uint16_t st = 0;
    if (tefGetOperationStatus(st) && st == target) return true;
    delay(10);
  }
  return false;
}

// ===================== Audio mute/unmute =====================
static bool g_manualMuted = false;   // heartbeat mute
static bool g_squelchMuted = false;  // reception squelch mute

static void tefSetMute(bool mute) {
  uint16_t p[] = { mute ? 1U : 0U };
  tefWriteWordsU16(0x30, 0x0B, 0x01, p, 1);
}

// ===================== STOP/START/ACTIVATE =====================
// "14 0000" / "14 0001" are undocumented low-level boot/idle transitions
// used by the Patches.h script + community firmwares. "14 0001" = Start
// (boot -> idle), matching UM §6.2 Fig 13. "14 0000" is used empirically
// to force a warm soft-reset back to boot state without toggling RST.
static void tefStop() {
  uint8_t buf[3] = { 0x14, 0x00, 0x00 };
  i2cWriteRawOK(buf, sizeof(buf));
}

static void tefStart() {
  uint8_t buf[3] = { 0x14, 0x00, 0x01 };
  i2cWriteRawOK(buf, sizeof(buf));
}
static void tefActivate() {
  // APPL cmd 5 Activate, mode=1 (idle -> active; radio standby)
  uint16_t p[] = { 1 };
  tefWriteWordsU16(0x40, 0x05, 0x01, p, 1);
}

// APPL cmd 4 Set_ReferenceClock (UM §3.40). Only valid in IDLE state,
// must be called AFTER Start (-> idle) and BEFORE Activate (-> active).
// Params: frequency_msb, frequency_lsb, type (0 = crystal, 1 = ext clock).
// Default is 9.216 MHz crystal, so calling this is technically optional
// when your module uses 9.216 MHz, but it's the manual-spec way and
// makes behaviour deterministic across warm resets.
static bool tefSetReferenceClock(uint32_t freq_hz, bool external) {
  uint16_t p[] = {
    (uint16_t)((freq_hz >> 16) & 0xFFFF),
    (uint16_t)(freq_hz & 0xFFFF),
    (uint16_t)(external ? 1 : 0)
  };
  return tefWriteWordsU16(0x40, 0x04, 0x01, p, 3);
}

// ===================== Patch loader =====================
static bool tefWrite1C(uint16_t value) {
  uint8_t buf[3] = { 0x1C, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
  return i2cWriteRawOK(buf, sizeof(buf));
}

static bool tefWrite1BChunk_P(const uint8_t* progmem, size_t offset, size_t len) {
  Wire.beginTransmission(TEF_I2C_ADDR);
  Wire.write((uint8_t)0x1B);
  for (size_t i=0;i<len;i++) {
    Wire.write(pgm_read_byte(&progmem[offset + i]));
  }
  return (Wire.endTransmission() == 0);
}

static bool tefUploadPatchAndLut() {
#ifdef DEBUG_SERIAL
  Serial.println(F("TEF: patch/LUT upload..."));
#endif

  // 1. Wake up / Prepare (matches Patches.h script header)
  if (!tefWrite1C(0x0000)) return false;
  if (!tefWrite1C(0x0074)) return false;

  // 2. Send Firmware Patch
  const size_t patchLen = sizeof(PatchByteValues);
  const size_t chunk = 24;

  for (size_t off=0; off<patchLen; off+=chunk) {
    size_t n = patchLen - off;
    if (n > chunk) n = chunk;
    if (n & 1) n--; // Must stay on 2-byte word boundary
    if (n == 0) break;
    if (!tefWrite1BChunk_P(PatchByteValues, off, n)) return false;
  }

  // 3. Prepare for LUT
  if (!tefWrite1C(0x0000)) return false;
  if (!tefWrite1C(0x0075)) return false;

  // 4. Send LUT
  const size_t lutLen = sizeof(LutByteValues);

  for (size_t off=0; off<lutLen; off+=chunk) {
    size_t n = lutLen - off;
    if (n > chunk) n = chunk;
    if (n & 1) n--;
    if (n == 0) break;
    if (!tefWrite1BChunk_P(LutByteValues, off, n)) return false;
  }

  // 5. Finalize
  if (!tefWrite1C(0x0000)) return false;

#ifdef DEBUG_SERIAL
  Serial.println(F("TEF: patch/LUT upload OK"));
#endif
  return true;
}

// ===================== FM profile (CAR OPTIMIZED, V102 manual compliant) =====================
// Every value below is sourced from the TEF668X User Manual V102 sections
// §3.3 (Set_Bandwidth), §3.7 (Set_MphSuppression), §3.8 (Set_ChannelEqualizer),
// §3.9 (Set_NoiseBlanker), §3.14 (Set_Deemphasis), §3.18 (Set_Softmute),
// §3.19 (Set_Highcut / Lowcut), §3.20 (Set_Stereo), §3.22 (Set_Scaler),
// §3.30 (Audio Set_Volume). "Field-perf" = UM's "suggested for improved
// field performance" recommendation.
static bool tefApplyFmCarProfile(int16_t vol_x01db) {

  // --- 1. Deemphasis (cmd 31) : 50 µs for Europe (unit = 0.1 µs)
  { uint16_t p[] = { 500 };
    if (!tefWriteWordsU16(0x20, 0x1F, 0x01, p, 1)) return false; }

  // --- 2. Bandwidth (cmd 10): auto PACS, 236 kHz max
  //      {mode=1 auto, bandwidth=2360 (0.1 kHz), ctrl_sens=1000 (100%), lo_sens=1000 (100%)}
  //      (was 3110 = 311 kHz in v5, too wide for Brussels FM density)
  { uint16_t p[] = { 1, 2360, 1000, 1000 };
    if (!tefWriteWordsU16(0x20, 0x0A, 0x01, p, 4)) return false; }

  // --- 2b. RF AGC (cmd 11): start level 89 dBuV, no step extension
  //      Range is 840..920 [*0.1 dBuV]. Default is 920 (92 dBuV).
  //      Lower start = earlier AGC kicks in = better intermod protection
  //      when driving near strong transmitters. 890 (89 dBuV) is the UM
  //      application example value -- good car compromise between
  //      desensitization (high start) and intermod (low start).
  //      UM §3.4 ATTENTION: Set_RFAGC must be >= 2 ms before next Tune_To;
  //      we call Tune_To at the end of this profile function, so safe.
  { uint16_t p[] = { 890, 0 };
    if (!tefWriteWordsU16(0x20, 0x0B, 0x01, p, 2)) return false; }

  // --- 3. Multipath Suppression (cmd 20): ON
  { uint16_t p[] = { 1 };
    if (!tefWriteWordsU16(0x20, 0x14, 0x01, p, 1)) return false; }

  // --- 4. Channel Equalizer (cmd 22): ON
  { uint16_t p[] = { 1 };
    if (!tefWriteWordsU16(0x20, 0x16, 0x01, p, 1)) return false; }

  // --- 5. Noise Blanker (cmd 23): ON, 100% sensitivity
  //      (was 1500 = 150% in v5 -> reduced to stop pumping on strong stations)
  { uint16_t p[] = { 1, 1000 };
    if (!tefWriteWordsU16(0x20, 0x17, 0x01, p, 2)) return false; }

  // =============== SoftMute (cmds 40–45) ===============
  // Softmute hides hiss under bridges / fringes / tunnels. KEEP IT ON.

  // cmd 40 Set_Softmute_Time (4 params): slow_attack, slow_decay, fast_attack, fast_decay
  //   FM field-perf: 120 ms / 500 ms slow, 1 ms / 2 ms fast
  { uint16_t p[] = { 120, 500, 10, 20 };
    if (!tefWriteWordsU16(0x20, 0x28, 0x01, p, 4)) return false; }

  // cmd 42 Set_Softmute_Level (3): mode, start[*0.1 dBuV], slope[*0.1 dB]
  //   mode 3 = dual timer, 15 dBuV start (FM default), 22 dB slope (FM default)
  { uint16_t p[] = { 3, 150, 220 };
    if (!tefWriteWordsU16(0x20, 0x2A, 0x01, p, 3)) return false; }

  // cmd 43 Set_Softmute_Noise (3): mode, start[*0.1 %USN], slope[*0.1 %]
  //   dual timer, 50% start / 100% slope (UM default)
  { uint16_t p[] = { 3, 500, 1000 };
    if (!tefWriteWordsU16(0x20, 0x2B, 0x01, p, 3)) return false; }

  // cmd 44 Set_Softmute_Mph (3): mode, start[*0.1 %WAM], slope[*0.1 %]
  { uint16_t p[] = { 3, 500, 1000 };
    if (!tefWriteWordsU16(0x20, 0x2C, 0x01, p, 3)) return false; }

  // cmd 45 Set_Softmute_Max (2): mode, limit[*0.1 dB]
  //   ON, 20 dB max attenuation (FM default). v5 had {0,0} which we removed.
  { uint16_t p[] = { 1, 200 };
    if (!tefWriteWordsU16(0x20, 0x2D, 0x01, p, 2)) return false; }

  // =============== HighCut (cmds 50–56) ===============
  // HighCut rolls off treble on weak / multipath signals to hide hiss.

  // cmd 50 Set_Highcut_Time (4): slow_atk, slow_dec, fast_atk, fast_dec
  //   UM field-perf: 200 ms slow_atk, 1 ms fast_atk, 8 ms fast_dec
  { uint16_t p[] = { 200, 2000, 10, 80 };
    if (!tefWriteWordsU16(0x20, 0x32, 0x01, p, 4)) return false; }

  // cmd 52 Set_Highcut_Level (3): mode, start[*0.1 dBuV], slope[*0.1 dB]
  //   dual timer, 36 dBuV start (FM default), 30 dB slope (FM default)
  { uint16_t p[] = { 3, 360, 300 };
    if (!tefWriteWordsU16(0x20, 0x34, 0x01, p, 3)) return false; }

  // cmd 53 Set_Highcut_Noise (3): mode, start[*0.1 %USN], slope[*0.1 %]
  //   UM field-perf: dual timer, 15% start, 20% slope
  { uint16_t p[] = { 3, 150, 200 };
    if (!tefWriteWordsU16(0x20, 0x35, 0x01, p, 3)) return false; }

  // cmd 54 Set_Highcut_Mph (3): mode, start[*0.1 %WAM], slope[*0.1 %]
  //   UM field-perf: dual timer, 12% start, 16% slope
  { uint16_t p[] = { 3, 120, 160 };
    if (!tefWriteWordsU16(0x20, 0x36, 0x01, p, 3)) return false; }

  // cmd 55 Set_Highcut_Max (2): mode, limit[Hz]
  //   UM field-perf: 2.4 kHz max highcut corner
  { uint16_t p[] = { 1, 2400 };
    if (!tefWriteWordsU16(0x20, 0x37, 0x01, p, 2)) return false; }

  // cmd 56 Set_Highcut_Min (2): mode, limit[Hz]
  //   ON, 10 kHz minimum corner on strong signals (keeps treble present)
  { uint16_t p[] = { 1, 10000 };
    if (!tefWriteWordsU16(0x20, 0x38, 0x01, p, 2)) return false; }

  // cmd 57 Set_Lowcut_Max (2): mode, limit[Hz]
  //   ON, 100 Hz max when signal is weak (cuts rumble from weak-signal distortion)
  { uint16_t p[] = { 1, 100 };
    if (!tefWriteWordsU16(0x20, 0x39, 0x01, p, 2)) return false; }

  // cmd 58 Set_Lowcut_Min (2): mode, limit[Hz]
  //   ON, 20 Hz fixed (DC/subsonic protection for car speakers/aux amp)
  { uint16_t p[] = { 1, 20 };
    if (!tefWriteWordsU16(0x20, 0x3A, 0x01, p, 2)) return false; }

  // cmd 59 Set_Highcut_Options (1): mode   [V102 only]
  //   mode 1 = IIR filter (default)
  { uint16_t p[] = { 1 };
    if (!tefWriteWordsU16(0x20, 0x3B, 0x01, p, 1)) return false; }

  // =============== Stereo blend (cmds 60–66) ===============
  // This is the CRITICAL section for the "multipath twitching" problem.
  // Stereo decoding amplifies multipath noise ~20x vs mono, so we want
  // the chip to blend to mono early when the signal gets rough.

  // cmd 60 Set_Stereo_Time (4): slow_atk, slow_dec, fast_atk, fast_dec
  //   UM example "field" values: 200 ms / 4000 ms slow, 2 ms / 8 ms fast
  //   (v5 had {1000, 6000, 20, 120} which was too snappy)
  { uint16_t p[] = { 200, 4000, 20, 80 };
    if (!tefWriteWordsU16(0x20, 0x3C, 0x01, p, 4)) return false; }

  // cmd 62 Set_Stereo_Level (3): mode, start[*0.1 dBuV], slope[*0.1 dB]
  //   dual timer, 46 dBuV start (UM default), 24 dB slope (UM default)
  //   v5 had start=30 dBuV which kept stereo on ALL weak stations ->
  //   multipath noise went straight to your ears. 46 dBuV blends much
  //   earlier = cleaner audio on marginal stations.
  { uint16_t p[] = { 3, 460, 240 };
    if (!tefWriteWordsU16(0x20, 0x3E, 0x01, p, 3)) return false; }

  // cmd 63 Set_Stereo_Noise (3): mode, start[*0.1 %USN], slope[*0.1 %]
  //   UM field-perf (§3.20 note): dual timer, 12% start, 16% slope
  //   Default would be {3, 240, 200}. Field-perf blends to mono much
  //   earlier on noise -> cleaner weak-signal reception in a moving car.
  { uint16_t p[] = { 3, 120, 160 };
    if (!tefWriteWordsU16(0x20, 0x3F, 0x01, p, 3)) return false; }

  // cmd 64 Set_Stereo_Mph (3): mode, start[*0.1 %WAM], slope[*0.1 %]
  //   UM field-perf (§3.20 note): dual timer, 10% start, 15% slope
  //   Aggressive blend to mono on multipath -- this is the other key
  //   fix for "move an inch and the sound changes" problem.
  { uint16_t p[] = { 3, 100, 150 };
    if (!tefWriteWordsU16(0x20, 0x40, 0x01, p, 3)) return false; }

  // cmd 65 Set_Stereo_Max (1): mode      ** BUG FIX: 1 parameter **
  //   mode 1 = dynamic control enabled. v5 passed wordCount=2 with a
  //   1-element array -> stack over-read sending garbage to the chip.
  { uint16_t p[] = { 1 };
    if (!tefWriteWordsU16(0x20, 0x41, 0x01, p, 1)) return false; }

  // cmd 66 Set_Stereo_Min (2): mode, limit[*0.1 c.s. dB]  — kept at default (skip)

  // cmd 70 Set_StHiBlend_Time (4): slow_atk, slow_dec, fast_atk, fast_dec
  //   Conservative/slow to match the stereo blend behaviour
  { uint16_t p[] = { 120, 500, 20, 80 };
    if (!tefWriteWordsU16(0x20, 0x46, 0x01, p, 4)) return false; }

  // StHiBlend narrows stereo channel separation on HIGH audio frequencies
  // when the signal gets rough. High-frequency stereo noise is the most
  // audible artifact of multipath, so making this aggressive cleans up
  // fringe reception noticeably without killing the stereo image entirely.

  // cmd 72 Set_StHiBlend_Level (3): mode, start[*0.1 dBuV], slope[*0.1 dB]
  //   dual timer, 60 dBuV start (UM default), 24 dB slope (UM default)
  { uint16_t p[] = { 3, 600, 240 };
    if (!tefWriteWordsU16(0x20, 0x48, 0x01, p, 3)) return false; }

  // cmd 73 Set_StHiBlend_Noise (3): mode, start[*0.1 %USN], slope[*0.1 %]
  //   UM field-perf (§3.21 note): 8% start (was 16% default), 14% slope
  { uint16_t p[] = { 3, 80, 140 };
    if (!tefWriteWordsU16(0x20, 0x49, 0x01, p, 3)) return false; }

  // cmd 74 Set_StHiBlend_Mph (3): mode, start[*0.1 %WAM], slope[*0.1 %]
  //   UM field-perf (§3.21 note): 8% start (was 16% default), 14% slope
  { uint16_t p[] = { 3, 80, 140 };
    if (!tefWriteWordsU16(0x20, 0x4A, 0x01, p, 3)) return false; }

  // cmd 75 Set_StHiBlend_Max (2): mode, limit[Hz]
  //   ON, 3 kHz max reduction of channel separation bandwidth
  //   (UM example value; default is 4 kHz, slightly less aggressive)
  { uint16_t p[] = { 1, 3000 };
    if (!tefWriteWordsU16(0x20, 0x4B, 0x01, p, 2)) return false; }

  // =============== RDS (cmd 81) ===============
  // Explicitly enable the RDS decoder. Default is mode=1 (decoder),
  // restart=2 (auto-restart after tune), interface=0 (I2C polling,
  // no GPIO pin). We're already polling Get_RDS_Data every 80 ms, so
  // this just guarantees the state across warm resets.
  { uint16_t p[] = { 1, 2, 0 };
    if (!tefWriteWordsU16(0x20, 0x51, 0x01, p, 3)) return false; }

  // ** v5 had a cmd 82 call here labelled "Noise Blanker". REMOVED. **
  // ** Cmd 82 = Set_QualityStatus, unrelated. Real NB is cmd 23 above. **

  // --- Scaler (cmd 80): 0 dB gain (signed)
  { int16_t p[] = { 0 };
    if (!tefWriteWordsS16(0x20, 0x50, 0x01, p, 1)) return false; }

  // --- Main audio volume (audio cmd 10)
  { int16_t p[] = { vol_x01db };
    if (!tefWriteWordsS16(0x30, 0x0A, 0x01, p, 1)) return false; }

  return true;
}

// ===================== Tuning + quality =====================
static void rdsReset(); // forward

static bool tefTuneFm(uint32_t freq_khz) {
  uint16_t f10 = (uint16_t)(freq_khz / 10); // Tune_To uses *10 kHz units
  // cmd 1 Tune_To: mode=1 (preset tune), frequency
  uint16_t p[] = { 1, f10 };
  bool ok = tefWriteWordsU16(0x20, 0x01, 0x01, p, 2);
  if (ok) rdsReset();
  return ok;
}

static bool tefGetQuality(uint16_t &status, int16_t &level_x01dBuV, uint16_t &usn_x01pct, uint16_t &wam_x01pct, int16_t &offset_x01kHz, uint16_t &bw_x01kHz, uint16_t &mod_x01pct) {
  // FM cmd 129 Get_Quality_Data: 7 words (status, level, usn, wam, offset, bw, mod)
  uint16_t w[7]={0};
  if (!tefReadWords(0x20, 0x81, 0x01, w, 7)) return false;
  status = w[0];
  level_x01dBuV = (int16_t)w[1];
  usn_x01pct = w[2];
  wam_x01pct = w[3];
  offset_x01kHz = (int16_t)w[4];
  bw_x01kHz = w[5];
  mod_x01pct = w[6];
  return true;
}

static uint8_t signalPercent() {
  // Uses TEF quality readout (0x20/0x81) and maps it to a conservative 0..100 "signal".
  // NOTE: "level" is 0.1 dBuV. Real FM receivers often show full bars only when
  // the antenna/system is genuinely good. A short wire near a strong transmitter
  // can still show high "level", so we additionally penalize by noise metrics.

  uint16_t qs=0, bw=0, mod=0;
  int16_t level=0, off=0;
  uint16_t usn=0, wam=0;
  if (!tefGetQuality(qs, level, usn, wam, off, bw, mod)) return 0;

  float lv_dBuV = level / 10.0f;  // dBuV

  // Conservative base mapping:
  //  - 10 dBuV => 0%
  //  - 50 dBuV => 100%
  float sig = (lv_dBuV - 10.0f) * (100.0f / 40.0f);
  if (sig < 0) sig = 0;
  if (sig > 100) sig = 100;

  // Penalize by noise / multipath (usn/wam are 0.1% units):
  float usnP = usn / 10.0f;
  float wamP = wam / 10.0f;

  float pen = 1.0f;
  if (usnP > 40.0f) pen *= 0.90f;
  if (usnP > 55.0f) pen *= 0.80f;
  if (usnP > 70.0f) pen *= 0.60f;

  if (wamP > 40.0f) pen *= 0.90f;
  if (wamP > 55.0f) pen *= 0.80f;
  if (wamP > 70.0f) pen *= 0.60f;

  int v = (int)(sig * pen + 0.5f);
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  return (uint8_t)v;
}


// ===================== RDS (PS + RadioText) logic =====================
// Prioritizes clean, stable output (no garbage characters) and smooth RT buildup.

static char g_ps[9] = "--------";
static char g_rt[65] = "";

static char ps_buf[9] = "--------";
static char ps_last_pub[9] = "--------";
static uint8_t ps_seen[8] = {0};   // stability counter per char position

static char rt_buf[65];
static char rt_last_pub[65];
static bool rt_ab=false;
static uint8_t rt_seen=0;

static void sanitizeText(char *s, uint8_t maxLen) {
  for (uint8_t i=0;i<maxLen;i++) {
    char c = s[i];
    if (c == '\0') { s[i] = '\0'; break; }
    if (c == '\r' || c == '\n') { s[i] = '\0'; break; }
    if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7E) s[i] = ' ';
  }
  int i = (int)strlen(s) - 1;
  while (i >= 0 && s[i] == ' ') { s[i] = '\0'; i--; }
}

static void rdsReset() {
  memset(ps_buf, '-', 8);
  ps_buf[8] = '\0';
  strcpy(g_ps, "Wait....");
  memset(ps_seen, 0, sizeof(ps_seen));

  memset(rt_buf, ' ', 64);
  rt_buf[64] = '\0';
  strcpy(g_rt, "Wait....");
  memset(rt_last_pub, 0, sizeof(rt_last_pub));
  rt_ab = false;
  rt_seen = 0;
}

static void publishPS() {
  if (strcmp(ps_last_pub, ps_buf) != 0) {
    strcpy(ps_last_pub, ps_buf);
    strcpy(g_ps, ps_buf);
  }
}

static void publishRT() {
  char tmp[65];
  strncpy(tmp, rt_buf, 64); tmp[64]='\0';
  sanitizeText(tmp, 64);
  if (strcmp(rt_last_pub, tmp) != 0) {
    strcpy(rt_last_pub, tmp);
    strcpy(g_rt, tmp);
  }
}

static void rdsProcessGroup(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  (void)a;
  uint8_t group = (uint8_t)((b >> 12) & 0x0F);
  bool verB = ((b >> 11) & 0x01) != 0;

  // ----- PS name: group 0A/0B -----
  if (group == 0) {
    uint8_t addr = (uint8_t)(b & 0x03);
    char c1 = (char)(d >> 8);
    char c2 = (char)(d & 0xFF);
    uint8_t pos = addr * 2;

    if (ps_buf[pos] == c1) { if (ps_seen[pos] < 10) ps_seen[pos]++; }
    else { if (ps_seen[pos] > 0) ps_seen[pos]--; else ps_buf[pos] = c1; }

    if (ps_buf[pos+1] == c2) { if (ps_seen[pos+1] < 10) ps_seen[pos+1]++; }
    else { if (ps_seen[pos+1] > 0) ps_seen[pos+1]--; else ps_buf[pos+1] = c2; }

    bool stable=true;
    for (uint8_t i=0;i<8;i++) if (ps_seen[i] < 5) { stable=false; break; }
    if (stable) publishPS();
  }

  // ----- RadioText: group 2A/2B -----
  if (group == 2) {
    bool ab = ((b >> 4) & 0x01) != 0;
    if (ab != rt_ab) {
      rt_ab = ab;
      memset(rt_buf, ' ', 64);
      rt_seen = 0;
      memset(rt_last_pub, 0, sizeof(rt_last_pub));
      g_rt[0]='\0';
    }

    if (!verB) {
      uint8_t seg = (uint8_t)(b & 0x0F);
      uint8_t base = seg * 4;
      if (base <= 60) {
        rt_buf[base+0] = (char)(c >> 8);
        rt_buf[base+1] = (char)(c & 0xFF);
        rt_buf[base+2] = (char)(d >> 8);
        rt_buf[base+3] = (char)(d & 0xFF);
        if (rt_seen < 16) rt_seen++;
        if (rt_seen >= 10) publishRT();
      }
    } else {
      uint8_t seg = (uint8_t)(b & 0x0F);
      uint8_t base = seg * 2;
      if (base <= 30) {
        rt_buf[base+0] = (char)(d >> 8);
        rt_buf[base+1] = (char)(d & 0xFF);
        if (rt_seen < 16) rt_seen++;
        if (rt_seen >= 10) publishRT();
      }
    }
  }
}

static void tefPollRds() {
  // FM cmd 131 Get_RDS_Data: 6 words (status, A, B, C, D, dec_error)
  uint16_t w[6]={0};
  if (!tefReadWords(0x20, 0x83, 0x01, w, 6)) return;

  uint16_t status = w[0];
  bool dataAvail = (status & 0x8000) != 0;
  bool sync      = (status & 0x0200) != 0;
  if (!sync || !dataAvail) return;

  uint16_t a=w[1], b=w[2], c=w[3], d=w[4];
  uint16_t dec_err=w[5];

  uint8_t ea = (dec_err >> 14) & 0x03;
  uint8_t eb = (dec_err >> 12) & 0x03;
  uint8_t ec = (dec_err >> 10) & 0x03;
  uint8_t ed = (dec_err >>  8) & 0x03;
  if (ea>=1 || eb>=1 || ec>=1 || ed>=1) return;

  rdsProcessGroup(a,b,c,d);
}


// ===================== Status print =====================
static void printStatusBlock() {
#ifdef DEBUG_SERIAL
  if (g_manualMuted || g_squelchMuted) return;

  Serial.print(F("Station Name: "));
  Serial.println(g_ps);

  Serial.print(F("Program info: "));
  Serial.println(g_rt);

  Serial.print(F("Signal:"));
  Serial.println(signalPercent());
  Serial.println();
#endif
}

// ===================== Squelch logic =====================
static void tefUpdateSquelch() {
  uint16_t qs=0, bw=0, mod=0;
  int16_t level=0, off=0;
  uint16_t usn=0, wam=0;
  if (!tefGetQuality(qs, level, usn, wam, off, bw, mod)) return;

  bool bad = ((usn > 650) || (wam > 650)) && (level < 350);
  bool good = ((usn < 450) && (wam < 450)) || (level > 420);

  if (!g_squelchMuted && bad) {
    g_squelchMuted = true;
    tefSetMute(true);
  } else if (g_squelchMuted && good) {
    g_squelchMuted = false;
    if (!g_manualMuted) tefSetMute(false);
  }
}

// ===================== Init logic =====================
static bool tefSoftInit(int16_t vol_x01db) {
  tefStop();
  delay(150);           // let the chip settle into boot state
  tefStart();
  // Poll for idle state (UM §6.1 recommendation), 500 ms max
  if (!tefWaitForState(1, 500)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: Start -> idle timeout"));
#endif
    return false;
  }
  // Idle state: set reference clock BEFORE Activate (UM §3.40).
  if (!tefSetReferenceClock(TEF_CRYSTAL_HZ, false)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: Set_ReferenceClock failed"));
#endif
    return false;
  }
  tefActivate();
  if (!tefWaitForState(2, 500)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: Activate -> active timeout"));
#endif
    return false;
  }

  Wire.setClock(I2C_SPEED_RUN);

  if (!tefApplyFmCarProfile(vol_x01db)) return false;
  if (!tefTuneFm(DEFAULT_FREQ_KHZ)) return false;

  rdsReset();
  g_manualMuted = true;
  tefSetMute(true);
  return true;
}

static bool tefHardInit(int16_t vol_x01db) {
  Wire.setClock(I2C_SPEED_PATCH);

  tefStop();
  delay(250);

  if (!tefUploadPatchAndLut()) return false;

  tefStart();
  if (!tefWaitForState(1, 800)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: HARD Start -> idle timeout"));
#endif
    return false;
  }
  // Idle state: set reference clock BEFORE Activate (UM §3.40).
  if (!tefSetReferenceClock(TEF_CRYSTAL_HZ, false)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: HARD Set_ReferenceClock failed"));
#endif
    return false;
  }
  tefActivate();
  if (!tefWaitForState(2, 800)) {
#ifdef DEBUG_SERIAL
    Serial.println(F("TEF: HARD Activate -> active timeout"));
#endif
    return false;
  }

  Wire.setClock(I2C_SPEED_RUN);

  if (!tefApplyFmCarProfile(vol_x01db)) return false;
  if (!tefTuneFm(DEFAULT_FREQ_KHZ)) return false;

  rdsReset();
  g_manualMuted = true;
  tefSetMute(true);
  return true;
}

static bool tefInitAuto(int16_t vol_x01db) {
#ifdef DEBUG_SERIAL
  Serial.println(F("TEF6686 Init: Starting..."));
#endif

  if (tefSoftInit(vol_x01db)) return true;
  delay(400);
  if (tefSoftInit(vol_x01db)) return true;

#ifdef DEBUG_SERIAL
  Serial.println(F("Soft init failed. Performing HARD init with patch..."));
#endif
  return tefHardInit(vol_x01db);
}

// ===================== Command parsing =====================
static int16_t g_vol = DEFAULT_VOL_X01DB;

static bool parseFreqToKHz(const String &s, uint32_t &out_khz) {
  String t = s;
  t.trim();
  if (t.length()==0) return false;
  t.replace(',', '.');

  if (t.indexOf('.') >= 0) {
    float mhz = t.toFloat();
    if (mhz < 87.5f || mhz > 108.0f) return false;
    out_khz = (uint32_t)lroundf(mhz * 1000.0f);
    return true;
  }

  long n = t.toInt();
  if (n <= 0) return false;

  if (n >= 87500 && n <= 108000) { out_khz = (uint32_t)n; return true; }      // kHz
  if (n >= 875 && n <= 1080)   { out_khz = (uint32_t)(n * 100); return true; } // 1047 -> 104700kHz
  return false;
}

// ===================== Serial UI + heartbeat =====================
static uint32_t g_lastHeartbeatMs = 0;
static uint32_t g_lastStatusPrintMs = 0;

static void onHeartbeat() {
  g_lastHeartbeatMs = millis();
  if (g_manualMuted) {
    g_manualMuted = false;
    if (!g_squelchMuted) tefSetMute(false);
  }
}

static void tuneButRespectMute(uint32_t khz) {
  tefTuneFm(khz);
  rdsReset();
  bool shouldMute = g_manualMuted || g_squelchMuted;
  tefSetMute(shouldMute);
}

static void handleSerial() {
#ifdef DEBUG_SERIAL
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (!s.length()) return;

  // Heartbeat
  if (s == "x" || s == "X") {
    onHeartbeat();
    return;
  }

  if (s=="q" || s=="Q") { if (!(g_manualMuted || g_squelchMuted)) printStatusBlock(); return; }
  if (s=="r" || s=="R") { rdsReset(); return; }

  // Volume: v-60 (0.1 dB steps, signed)
  if (s.startsWith("v") || s.startsWith("V")) {
    int v = s.substring(1).toInt();
    g_vol = (int16_t)v;
    int16_t p[] = { g_vol };
    if (tefWriteWordsS16(0x30, 0x0A, 0x01, p, 1)) {
      if (!(g_manualMuted || g_squelchMuted)) {
        Serial.print(F("Volume: ")); Serial.print(g_vol/10.0); Serial.println(F(" dB"));
      }
    } else {
      if (!(g_manualMuted || g_squelchMuted)) Serial.println(F("VOL set failed"));
    }
    return;
  }

  if (s=="reinit") {
    if (!(g_manualMuted || g_squelchMuted)) Serial.println(F("SOFT reinit..."));
    if (!tefSoftInit(g_vol)) {
      if (!(g_manualMuted || g_squelchMuted)) Serial.println(F("SOFT reinit failed"));
    }
    return;
  }
  if (s=="hardinit") {
    if (!(g_manualMuted || g_squelchMuted)) Serial.println(F("HARD init..."));
    if (!tefHardInit(g_vol)) {
      if (!(g_manualMuted || g_squelchMuted)) Serial.println(F("HARD init failed"));
    }
    return;
  }

  // Otherwise: treat as frequency (no prefix required)
  uint32_t khz=0;
  if (parseFreqToKHz(s, khz)) {
    tuneButRespectMute(khz);
    return;
  }

  if (!(g_manualMuted || g_squelchMuted)) {
    Serial.println(F("Cmds: x, <freq> (104.7/1047/104700), v-60, q, r, reinit, hardinit"));
  }
#endif
}

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  delay(50);
#endif
  Wire.begin();
  Wire.setWireTimeout(25000, true);
  Wire.setClock(I2C_SPEED_RUN);

  g_lastHeartbeatMs = 0;
  g_manualMuted = true;
  g_squelchMuted = false;

  rdsReset();

  if (!tefInitAuto(g_vol)) {
    Serial.println(F("Init failed, retrying..."));
    while (!tefHardInit(g_vol)) delay(1000);
  }

  tefSetMute(true);
  g_lastStatusPrintMs = millis();

#ifdef DEBUG_SERIAL
  Serial.println(F("Ready."));
  Serial.println(F("Send x every <4s to unmute."));
  Serial.println(F("Tune: 104.7 or 1047 or 104700"));
#endif
}

void loop() {
  handleSerial();

  uint32_t now = millis();

  // Heartbeat timeout -> mute
  if (!g_manualMuted) {
    if (g_lastHeartbeatMs == 0 || (now - g_lastHeartbeatMs) > HEARTBEAT_TIMEOUT_MS) {
      g_manualMuted = true;
      tefSetMute(true);
    }
  }

  // Squelch update (currently disabled; enable once you've verified the profile)
  static uint32_t tQ=0;
  if (now - tQ >= 200) {
    tQ = now;
    // tefUpdateSquelch();
  }

  // RDS poll
  static uint32_t tR=0;
  if (now - tR >= 80) {
    tR = now;
    tefPollRds();
  }

  // Status print every 5s (only when not muted)
  if (!(g_manualMuted || g_squelchMuted)) {
    if (now - g_lastStatusPrintMs >= STATUS_PRINT_MS) {
      g_lastStatusPrintMs = now;
      printStatusBlock();
    }
  }
}
