/*
  audio_ducking - lower the radio smoothly while something else must be heard
  (voice prompt, parking sensor, phone, navigation...), then bring it back.

  Trigger: DUCK_PIN pulled LOW (e.g. the BUSY pin of a DFPlayer / voice module,
  LOW while it plays), or send 'd' on the serial monitor to toggle by hand.

  How it works: the TEF volume (0.1 dB steps) is moved one step at a time from
  loop() without delay(), so the fade never blocks the rest of the sketch.
  The radio keeps playing underneath; it is only made quieter.

  Settings to adjust:
    NORMAL_VOLUME  listening level                      (0.1 dB, 0 = no gain)
    DUCK_VOLUME    level while ducked                   (0.1 dB)
    FADE_DOWN_MS   time to go from normal to duck level
    FADE_UP_MS     time to come back (slower sounds more natural)
    RELEASE_MS     wait after the trigger ends before coming back
*/
#include <Wire.h>
#include <TEF668X.h>

TEF668X radio;

const uint8_t DUCK_PIN = 4;                        // LOW = duck
const int16_t NORMAL_VOLUME = TEF_VOLUME_MINUS_10DB;  // -10 dB
const int16_t DUCK_VOLUME = (-300);                // -30 dB: radio still audible underneath
const uint16_t FADE_DOWN_MS = 300;
const uint16_t FADE_UP_MS = 1500;
const uint16_t RELEASE_MS = 500;

int16_t currentVolume = NORMAL_VOLUME;   // what the chip has now (0.1 dB)
int16_t targetVolume = NORMAL_VOLUME;    // where the fade goes
uint32_t lastStepMs = 0;
uint32_t triggerEndMs = 0;
bool manualDuck = false;

// Time between two 0.1 dB steps so a full fade takes fadeMs.
uint16_t stepInterval(uint16_t fadeMs) {
  const int16_t span = NORMAL_VOLUME - DUCK_VOLUME;   // in 0.1 dB steps
  return span > 0 ? fadeMs / span : 1;
}

// Move the volume one step towards the target when it is time. Never blocks.
void updateFade() {
  if (currentVolume == targetVolume) return;
  const bool goingDown = targetVolume < currentVolume;
  const uint16_t interval = stepInterval(goingDown ? FADE_DOWN_MS : FADE_UP_MS);
  if (millis() - lastStepMs < interval) return;
  lastStepMs = millis();
  currentVolume += goingDown ? -1 : 1;
  radio.setVolume(currentVolume);
}

// Decide the target from the trigger(s).
void updateTrigger() {
  const bool triggered = manualDuck || digitalRead(DUCK_PIN) == LOW;
  if (triggered) {
    targetVolume = DUCK_VOLUME;      // duck at once
    triggerEndMs = millis();
  } else if (millis() - triggerEndMs >= RELEASE_MS) {
    targetVolume = NORMAL_VOLUME;    // come back after the release delay
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(DUCK_PIN, INPUT_PULLUP);
  Wire.begin();

  radio.begin(Wire);
  if (radio.init() != TEF_OK) {
    Serial.println(F("TEF668X init failed"));
    while (true) {}
  }
  radio.applyPreset(TEF_PRESET_NXP_FIELD_FM);
  radio.tuneFM(10470);                       // 104.70 MHz
  radio.setVolume(currentVolume);
  radio.setMute(TEF_OFF);
  Serial.println(F("'d' = toggle ducking"));
}

void loop() {
  if (Serial.available() && Serial.read() == 'd') {
    manualDuck = !manualDuck;
    Serial.println(manualDuck ? F("duck") : F("release"));
  }
  updateTrigger();
  updateFade();
  // ... the rest of your sketch runs freely here
}
