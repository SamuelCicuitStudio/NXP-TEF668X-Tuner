/**
 * @file tef6686_full_console.ino
 * @brief Full TEF6686 console-style example using the TEF668X library.
 *
 * @details
 * This example mirrors the behavior of `ref/tef6686done.ino` but uses the
 * TEF668X library API for init/tune/audio/profile and low-level reads for RDS.
 *
 * Serial commands at 115200:
 * - `x`                : heartbeat keepalive (unmutes)
 * - `104.7` / `1047` / `104700` / `f104.7` : tune FM
 * - `v-60`             : set volume in 0.1 dB (signed)
 * - `q`                : print one status block now
 * - `r`                : reset RDS buffers
 * - `reinit`           : soft init (no patch upload)
 * - `hardinit`         : hard init (patch upload)
 */

#include <Arduino.h>
#include <Wire.h>
#include <TEF668X.h>

static TEF668X radio;

static const uint8_t TEF_I2C_ADDR = 0x64;
static const uint32_t I2C_SPEED_PATCH = 100000UL;
static const uint32_t I2C_SPEED_RUN = 100000UL;
static const uint32_t TEF_CRYSTAL_HZ = 9216000UL;
static const uint16_t DEFAULT_FREQ_X10KHZ = 10470; // 104.70 MHz
static const int16_t DEFAULT_VOL_X01DB = -80; // -8.0 dB

static const uint32_t HEARTBEAT_TIMEOUT_MS = 4000UL;
static const uint32_t STATUS_PRINT_MS = 5000UL;
static const bool ENABLE_SQUELCH = false;

static bool g_manualMuted = true;   // heartbeat mute
static bool g_squelchMuted = false; // reception squelch mute
static int16_t g_vol = DEFAULT_VOL_X01DB;

static uint32_t g_lastHeartbeatMs = 0;
static uint32_t g_lastStatusPrintMs = 0;

static char g_ps[9] = "--------";
static char g_rt[65] = "";
static char ps_buf[9] = "--------";
static char ps_last_pub[9] = "--------";
static uint8_t ps_seen[8] = {0};
static char rt_buf[65];
static char rt_last_pub[65];
static bool rt_ab = false;
static uint8_t rt_seen = 0;

static void rdsReset();

static void printError(const char* label, TEF668X::Error e) {
  Serial.print(label);
  Serial.print(" failed, err=");
  Serial.println(static_cast<uint8_t>(e));
}

static bool tefSetMute(bool mute) {
  return radio.setMute(mute) == TEF668X::Error::Ok;
}

static bool tefGetOperationStatus(uint16_t& status) {
  return radio.getOperationStatus(status) == TEF668X::Error::Ok;
}

static bool tuneFmX10kHz(uint16_t freq_x10kHz) {
  if (radio.tuneFmPreset(freq_x10kHz) != TEF668X::Error::Ok) return false;
  rdsReset();
  return true;
}

static bool parseFreqToX10kHz(const String& raw, uint16_t& out_x10kHz) {
  String s = raw;
  s.trim();
  if (!s.length()) return false;

  if (s.startsWith("f") || s.startsWith("F")) {
    s = s.substring(1);
    s.trim();
  }

  s.replace(',', '.');

  if (s.indexOf('.') >= 0) {
    const float mhz = s.toFloat();
    if (mhz < 87.5f || mhz > 108.0f) return false;
    out_x10kHz = static_cast<uint16_t>(mhz * 100.0f + 0.5f);
    return true;
  }

  const long n = s.toInt();
  if (n <= 0) return false;

  if (n >= 87500 && n <= 108000) {       // kHz
    out_x10kHz = static_cast<uint16_t>(n / 10);
    return true;
  }
  if (n >= 8750 && n <= 10800) {         // already x10kHz
    out_x10kHz = static_cast<uint16_t>(n);
    return true;
  }
  if (n >= 875 && n <= 1080) {           // 1047 -> 10470
    out_x10kHz = static_cast<uint16_t>(n * 10);
    return true;
  }
  return false;
}

static bool applyRdsDefaults() {
  // FM cmd 81 Set_RDS: mode=1 decoder, restart=2 auto restart, interface=0
  const uint16_t p[] = {1, 2, 0};
  return radio.writeWordsU16(0x20, 0x51, 0x01, p, 3) == TEF668X::Error::Ok;
}

static bool postInitAndTune(int16_t vol_x01db) {
  if (radio.applyFmCarProfile(vol_x01db) != TEF668X::Error::Ok) return false;
  if (!applyRdsDefaults()) return false;
  if (!tuneFmX10kHz(DEFAULT_FREQ_X10KHZ)) return false;

  g_manualMuted = true;
  g_squelchMuted = false;
  tefSetMute(true);
  return true;
}

static bool tefSoftInit(int16_t vol_x01db) {
  const TEF668X::Error e = radio.initSoft(TEF_CRYSTAL_HZ, false, 900);
  if (e != TEF668X::Error::Ok) return false;
  return postInitAndTune(vol_x01db);
}

static bool tefHardInit(int16_t vol_x01db) {
  const TEF668X::Error e = radio.initHard(TEF_CRYSTAL_HZ, false, 1400);
  if (e != TEF668X::Error::Ok) return false;
  return postInitAndTune(vol_x01db);
}

static bool tefInitAuto(int16_t vol_x01db) {
  if (tefSoftInit(vol_x01db)) return true;
  delay(400);
  if (tefSoftInit(vol_x01db)) return true;
  return tefHardInit(vol_x01db);
}

static bool tefGetQuality(uint16_t& status,
                          int16_t& level_x01dBuV,
                          uint16_t& usn_x01pct,
                          uint16_t& wam_x01pct,
                          int16_t& offset_x01kHz,
                          uint16_t& bw_x01kHz,
                          uint16_t& mod_x01pct) {
  TEF668X::QualityData q;
  const TEF668X::Error e = radio.getQualityFm(q);
  if (e != TEF668X::Error::Ok) return false;
  status = q.status;
  level_x01dBuV = q.level_x10_dBuV;
  usn_x01pct = q.usn_x10_pct;
  wam_x01pct = q.wam_x10_pct;
  offset_x01kHz = q.offset_x10_kHz;
  bw_x01kHz = q.bandwidth_x10_kHz;
  mod_x01pct = q.modulation_x10_pct;
  return true;
}

static uint8_t signalPercent() {
  uint16_t qs = 0;
  int16_t level = 0;
  uint16_t usn = 0, wam = 0, bw = 0, mod = 0;
  int16_t off = 0;
  if (!tefGetQuality(qs, level, usn, wam, off, bw, mod)) return 0;

  float lv_dBuV = level / 10.0f;
  float sig = (lv_dBuV - 10.0f) * (100.0f / 40.0f);
  if (sig < 0.0f) sig = 0.0f;
  if (sig > 100.0f) sig = 100.0f;

  const float usnP = usn / 10.0f;
  const float wamP = wam / 10.0f;
  float pen = 1.0f;

  if (usnP > 40.0f) pen *= 0.90f;
  if (usnP > 55.0f) pen *= 0.80f;
  if (usnP > 70.0f) pen *= 0.60f;

  if (wamP > 40.0f) pen *= 0.90f;
  if (wamP > 55.0f) pen *= 0.80f;
  if (wamP > 70.0f) pen *= 0.60f;

  int v = static_cast<int>(sig * pen + 0.5f);
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  return static_cast<uint8_t>(v);
}

static void sanitizeText(char* s, uint8_t maxLen) {
  for (uint8_t i = 0; i < maxLen; i++) {
    char c = s[i];
    if (c == '\0') break;
    if (c == '\r' || c == '\n') {
      s[i] = '\0';
      break;
    }
    if (static_cast<uint8_t>(c) < 0x20 || static_cast<uint8_t>(c) > 0x7E) s[i] = ' ';
  }
  int i = static_cast<int>(strlen(s)) - 1;
  while (i >= 0 && s[i] == ' ') {
    s[i] = '\0';
    i--;
  }
}

static void publishPS() {
  if (strcmp(ps_last_pub, ps_buf) != 0) {
    strcpy(ps_last_pub, ps_buf);
    strcpy(g_ps, ps_buf);
  }
}

static void publishRT() {
  char tmp[65];
  strncpy(tmp, rt_buf, 64);
  tmp[64] = '\0';
  sanitizeText(tmp, 64);
  if (strcmp(rt_last_pub, tmp) != 0) {
    strcpy(rt_last_pub, tmp);
    strcpy(g_rt, tmp);
  }
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

static void rdsProcessGroup(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  (void)a;
  const uint8_t group = static_cast<uint8_t>((b >> 12) & 0x0F);
  const bool verB = ((b >> 11) & 0x01) != 0;

  if (group == 0) {
    const uint8_t addr = static_cast<uint8_t>(b & 0x03);
    const char c1 = static_cast<char>(d >> 8);
    const char c2 = static_cast<char>(d & 0xFF);
    const uint8_t pos = addr * 2;

    if (ps_buf[pos] == c1) {
      if (ps_seen[pos] < 10) ps_seen[pos]++;
    } else {
      if (ps_seen[pos] > 0) ps_seen[pos]--;
      else ps_buf[pos] = c1;
    }

    if (ps_buf[pos + 1] == c2) {
      if (ps_seen[pos + 1] < 10) ps_seen[pos + 1]++;
    } else {
      if (ps_seen[pos + 1] > 0) ps_seen[pos + 1]--;
      else ps_buf[pos + 1] = c2;
    }

    bool stable = true;
    for (uint8_t i = 0; i < 8; i++) {
      if (ps_seen[i] < 5) {
        stable = false;
        break;
      }
    }
    if (stable) publishPS();
  }

  if (group == 2) {
    const bool ab = ((b >> 4) & 0x01) != 0;
    if (ab != rt_ab) {
      rt_ab = ab;
      memset(rt_buf, ' ', 64);
      rt_seen = 0;
      memset(rt_last_pub, 0, sizeof(rt_last_pub));
      g_rt[0] = '\0';
    }

    if (!verB) {
      const uint8_t seg = static_cast<uint8_t>(b & 0x0F);
      const uint8_t base = seg * 4;
      if (base <= 60) {
        rt_buf[base + 0] = static_cast<char>(c >> 8);
        rt_buf[base + 1] = static_cast<char>(c & 0xFF);
        rt_buf[base + 2] = static_cast<char>(d >> 8);
        rt_buf[base + 3] = static_cast<char>(d & 0xFF);
        if (rt_seen < 16) rt_seen++;
        if (rt_seen >= 10) publishRT();
      }
    } else {
      const uint8_t seg = static_cast<uint8_t>(b & 0x0F);
      const uint8_t base = seg * 2;
      if (base <= 30) {
        rt_buf[base + 0] = static_cast<char>(d >> 8);
        rt_buf[base + 1] = static_cast<char>(d & 0xFF);
        if (rt_seen < 16) rt_seen++;
        if (rt_seen >= 10) publishRT();
      }
    }
  }
}

static void tefPollRds() {
  uint16_t w[6] = {0, 0, 0, 0, 0, 0};
  if (radio.readWords(0x20, 0x83, 0x01, w, 6) != TEF668X::Error::Ok) return;

  const uint16_t status = w[0];
  const bool dataAvail = (status & 0x8000U) != 0;
  const bool sync = (status & 0x0200U) != 0;
  if (!sync || !dataAvail) return;

  const uint16_t a = w[1], b = w[2], c = w[3], d = w[4];
  const uint16_t decErr = w[5];

  const uint8_t ea = static_cast<uint8_t>((decErr >> 14) & 0x03);
  const uint8_t eb = static_cast<uint8_t>((decErr >> 12) & 0x03);
  const uint8_t ec = static_cast<uint8_t>((decErr >> 10) & 0x03);
  const uint8_t ed = static_cast<uint8_t>((decErr >> 8) & 0x03);
  if (ea >= 1 || eb >= 1 || ec >= 1 || ed >= 1) return;

  rdsProcessGroup(a, b, c, d);
}

static void printStatusBlock() {
  if (g_manualMuted || g_squelchMuted) return;

  uint16_t op = 0;
  if (tefGetOperationStatus(op)) {
    Serial.print(F("OpState: "));
    Serial.println(op);
  }
  Serial.print(F("Station Name: "));
  Serial.println(g_ps);
  Serial.print(F("Program info: "));
  Serial.println(g_rt);
  Serial.print(F("Signal: "));
  Serial.println(signalPercent());
  Serial.println();
}

static void tefUpdateSquelch() {
  uint16_t qs = 0;
  int16_t level = 0;
  uint16_t usn = 0, wam = 0, bw = 0, mod = 0;
  int16_t off = 0;
  if (!tefGetQuality(qs, level, usn, wam, off, bw, mod)) return;

  const bool bad = ((usn > 650U) || (wam > 650U)) && (level < 350);
  const bool good = ((usn < 450U) && (wam < 450U)) || (level > 420);

  if (!g_squelchMuted && bad) {
    g_squelchMuted = true;
    tefSetMute(true);
  } else if (g_squelchMuted && good) {
    g_squelchMuted = false;
    if (!g_manualMuted) tefSetMute(false);
  }
}

static void onHeartbeat() {
  g_lastHeartbeatMs = millis();
  if (g_manualMuted) {
    g_manualMuted = false;
    if (!g_squelchMuted) tefSetMute(false);
  }
}

static void tuneButRespectMute(uint16_t freq_x10kHz) {
  if (!tuneFmX10kHz(freq_x10kHz)) return;
  const bool shouldMute = g_manualMuted || g_squelchMuted;
  tefSetMute(shouldMute);
}

static void handleSerial() {
  if (!Serial.available()) return;

  String s = Serial.readStringUntil('\n');
  s.trim();
  if (!s.length()) return;

  if (s == "x" || s == "X") {
    onHeartbeat();
    return;
  }
  if (s == "q" || s == "Q") {
    printStatusBlock();
    return;
  }
  if (s == "r" || s == "R") {
    rdsReset();
    return;
  }

  if (s.startsWith("v") || s.startsWith("V")) {
    const int v = s.substring(1).toInt();
    g_vol = static_cast<int16_t>(v);
    if (radio.setVolumeDb10(g_vol) == TEF668X::Error::Ok) {
      if (!(g_manualMuted || g_squelchMuted)) {
        Serial.print(F("Volume: "));
        Serial.print(g_vol / 10.0f);
        Serial.println(F(" dB"));
      }
    } else {
      printError("setVolumeDb10", TEF668X::Error::I2CError);
    }
    return;
  }

  if (s == "reinit") {
    if (!tefSoftInit(g_vol)) Serial.println(F("SOFT reinit failed"));
    return;
  }
  if (s == "hardinit") {
    if (!tefHardInit(g_vol)) Serial.println(F("HARD init failed"));
    return;
  }

  uint16_t freq_x10kHz = 0;
  if (parseFreqToX10kHz(s, freq_x10kHz)) {
    tuneButRespectMute(freq_x10kHz);
    return;
  }

  if (!(g_manualMuted || g_squelchMuted)) {
    Serial.println(F("Cmds: x, <freq> (104.7/1047/104700/f104.7), v-60, q, r, reinit, hardinit"));
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  Wire.begin();

  radio.begin(Wire, TEF_I2C_ADDR);
  radio.setI2CSpeeds(I2C_SPEED_PATCH, I2C_SPEED_RUN);
  radio.setPatchChunkSize(24);
  radio.setVariantPolicy(TEF668X::VariantPolicy::ForceTEF6686);

  g_lastHeartbeatMs = 0;
  g_manualMuted = true;
  g_squelchMuted = false;
  rdsReset();

  if (!tefInitAuto(g_vol)) {
    Serial.println(F("Init failed, retrying hard init..."));
    while (!tefHardInit(g_vol)) delay(1000);
  }

  tefSetMute(true);
  g_lastStatusPrintMs = millis();

  Serial.println(F("Ready."));
  Serial.println(F("Send x every <4s to unmute."));
  Serial.println(F("Tune: 104.7 or 1047 or 104700 or f104.7"));
}

void loop() {
  handleSerial();

  const uint32_t now = millis();

  if (!g_manualMuted) {
    if (g_lastHeartbeatMs == 0 || (now - g_lastHeartbeatMs) > HEARTBEAT_TIMEOUT_MS) {
      g_manualMuted = true;
      tefSetMute(true);
    }
  }

  static uint32_t tQ = 0;
  if (now - tQ >= 200) {
    tQ = now;
    if (ENABLE_SQUELCH) tefUpdateSquelch();
  }

  static uint32_t tR = 0;
  if (now - tR >= 80) {
    tR = now;
    tefPollRds();
  }

  if (!(g_manualMuted || g_squelchMuted)) {
    if (now - g_lastStatusPrintMs >= STATUS_PRINT_MS) {
      g_lastStatusPrintMs = now;
      printStatusBlock();
    }
  }
}
