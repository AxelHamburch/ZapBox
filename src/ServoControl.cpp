#include "ServoControl.h"
#include "GlobalState.h"
#include "PinConfig.h"
#include "Log.h"
#include <ESP32Servo.h>
#include <Preferences.h>

static Servo servo1;  // Display: Pin 13 positional | Headless: Pin 12 (servo180)
static Servo servo2;  // Display: Pin 10 continuous  | Headless: Pin 12 (servo360)
static bool servo1Attached = false;
static bool servo2Attached = false;

// Positional (180°) servos: attach is deferred until the first move so nothing drives
// the pin during boot. The last commanded position is kept in NVS per servo; at boot
// a servo is only homed to Start if it is not known to be there already.
static bool servo1Enabled = false;     // configured and usable, not necessarily attached yet
static const char* SERVO_NVS_NS = "servopos";

static int servo1Pin() {
#if ENABLE_DISPLAY
  return 13;
#else
  return 12;
#endif
}

static int loadServoPos(const char* key) {
  Preferences p;
  if (!p.begin(SERVO_NVS_NS, true)) return -1;
  int v = p.getInt(key, -1);
  p.end();
  return v;
}

static void saveServoPos(const char* key, int pos) {
  Preferences p;
  if (!p.begin(SERVO_NVS_NS, false)) return;
  p.putInt(key, pos);
  p.end();
}

// Attach on demand and assert Start before the first sweep (no-op if already there).
static void ensureServo180Attached(Servo& srv, int pin, int start, const char* name) {
  if (srv.attached()) return;
  srv.attach(pin);
  srv.write(start);
  LOG_INFO("Servo", String(name) + " attached to Pin " + pin + ", start: " + start + "°");
}

// Boot: leave the pin untouched while the position is known to be Start; only
// drive the servo (to Start) if it may have been left elsewhere.
static void initServo180Deferred(Servo& srv, int pin, int start, const char* key, const char* name) {
  int known = loadServoPos(key);
  if (known == start) {
    LOG_INFO("Servo", String(name) + " already at " + known + "° (stored) - not driven at boot");
    return;
  }
  LOG_INFO("Servo", String(name) + " position unknown/changed (stored " + known + "°) - homing to start");
  ensureServo180Attached(srv, pin, start, name);
  saveServoPos(key, start);
}

#ifdef BOARD_ESP32C3_21_1
static Servo servo_flex6;  // C3 GPIO6 flex channel
static Servo servo_flex7;  // C3 GPIO7 flex channel
static bool servoFlex6Attached = false;
static bool servoFlex7Attached = false;
#endif

#ifdef BOARD_JC3248W535C
// Touch 3.5 multi-channel: one servo per flex channel (CH01..CH06).
// Index 0..5 maps to GPIO 14, 15, 16, 5, 6, 7 (== RELAY_CHANNEL_PINS).
static const int T35_SERVO_GPIO[6] = {14, 15, 16, 5, 6, 7};
static Servo t35Servos[6];
static bool  t35Attached[6] = {false, false, false, false, false, false};
static int t35ServoIdx(int gpio) {
  for (int i = 0; i < 6; i++) if (T35_SERVO_GPIO[i] == gpio) return i;
  return -1;
}
#endif

// Slowly sweep a servo from one angle to another over the given duration.
// If duration_ms == 0, jump directly to target (native servo speed).
static void slowSweep(Servo &srv, int fromAngle, int toAngle, int duration_ms) {
  int steps = abs(toAngle - fromAngle);
  if (steps == 0) {
    srv.write(toAngle);
    return;
  }
  if (duration_ms <= 0) {
    srv.write(toAngle);
    return;
  }
  int stepDelay = duration_ms / steps;
  if (stepDelay < 1) stepDelay = 1;
  int dir = (toAngle > fromAngle) ? 1 : -1;
  for (int pos = fromAngle; pos != toAngle; pos += dir) {
    srv.write(pos);
    vTaskDelay(pdMS_TO_TICKS(stepDelay));
  }
  srv.write(toAngle);
}

// Sweep Start -> End; the stored position is End until the servo is back at Start.
static void sweepOut180(Servo& srv, int pin, int start, int end, int dur, const char* key, const char* name) {
  ensureServo180Attached(srv, pin, start, name);
  saveServoPos(key, end);
  slowSweep(srv, start, end, dur);
}

static void sweepBack180(Servo& srv, int pin, int start, int end, int dur, const char* key, const char* name) {
  ensureServo180Attached(srv, pin, start, name);
  slowSweep(srv, end, start, dur);
  saveServoPos(key, start);
}

void initServos() {
#if ENABLE_DISPLAY
  // Display version: servo1 on Pin 13, servo2 on Pin 10
  if (servoConfig.servo1Active()) {
    servo1Enabled = true;
    initServo180Deferred(servo1, servo1Pin(), servoConfig.servo1Start, "pos1", "180° Servo");
  }
  if (servoConfig.servo2Active()) {
    servo2.attach(10);
    servo2Attached = true;
    servo2.write(90); // 90 = stop for continuous servo
    LOG_INFO("Servo", String("Servo 2 (continuous) attached to Pin 10, stopped (90)"));
  }
#else
  // Headless version: servo on Pin 12 (single-pin mode)
  if (multiChannelConfig.mode == "servo180" && servoConfig.servo1Active()) {
    servo1Enabled = true;
    initServo180Deferred(servo1, servo1Pin(), servoConfig.servo1Start, "pos1", "180° Servo");
  } else if (multiChannelConfig.mode == "servo360" && servoConfig.servo2Active()) {
    servo2.attach(12);
    servo2Attached = true;
    servo2.write(90); // 90 = stop for continuous servo
    LOG_INFO("Servo", "360° Servo attached to Pin 12, stopped (90)");
  }
#endif
#ifdef BOARD_ESP32C3_21_1
  // 180°: attach deferred (flag = enabled); 360°: attach + stop (90) as before
  if (c3FlexConfig.gpio6Servo180 || c3FlexConfig.gpio6Servo360) {
    servoFlex6Attached = true;
    if (c3FlexConfig.gpio6Servo180) {
      initServo180Deferred(servo_flex6, PIN_FLEX_CH01, c3FlexConfig.gpio6S180Start, "pos_f6", "Flex servo 6");
    } else {
      servo_flex6.attach(PIN_FLEX_CH01);
      servo_flex6.write(90);
      LOG_INFO("Servo", "Flex GPIO6 servo attached, start: 90 (stop)");
    }
  }
  if (c3FlexConfig.gpio7Servo180 || c3FlexConfig.gpio7Servo360) {
    servoFlex7Attached = true;
    if (c3FlexConfig.gpio7Servo180) {
      initServo180Deferred(servo_flex7, PIN_FLEX_CH02, c3FlexConfig.gpio7S180Start, "pos_f7", "Flex servo 7");
    } else {
      servo_flex7.attach(PIN_FLEX_CH02);
      servo_flex7.write(90);
      LOG_INFO("Servo", "Flex GPIO7 servo attached, start: 90 (stop)");
    }
  }
#endif
#ifdef BOARD_JC3248W535C
  for (int i = 0; i < 6; i++) {
    const T35AmbientConfig::ServoChannel& sc = t35AmbientConfig.servo[i];
    if (!sc.isServo()) continue;
    t35Attached[i] = true;
    if (sc.servo180) {
      initServo180Deferred(t35Servos[i], T35_SERVO_GPIO[i], sc.s180Start, (String("pos_t") + i).c_str(), "T35 servo");
    } else {
      t35Servos[i].attach(T35_SERVO_GPIO[i]);
      t35Servos[i].write(90); // 90 = stop for continuous
      LOG_INFO("Servo", String("T35 GPIO") + T35_SERVO_GPIO[i] + " servo attached, start: 90 (stop)");
    }
  }
#endif
}

void activateServo(int servoPin) {
#ifdef BOARD_JC3248W535C
  {
    int i = t35ServoIdx(servoPin);
    if (i >= 0 && t35Attached[i]) {
      const T35AmbientConfig::ServoChannel& sc = t35AmbientConfig.servo[i];
      if (sc.servo180) {
        LOG_INFO("Servo", String("T35 GPIO") + servoPin + " 180°: " + sc.s180Start + "° → " + sc.s180End + "°");
        sweepOut180(t35Servos[i], T35_SERVO_GPIO[i], sc.s180Start, sc.s180End, sc.s180Duration, (String("pos_t") + i).c_str(), "T35 servo");
      } else if (sc.servo360) {
        LOG_INFO("Servo", String("T35 GPIO") + servoPin + " 360°: speed " + sc.s360Speed);
        t35Servos[i].write(sc.s360Speed);
        if (sc.s360Duration > 0) {
          vTaskDelay(pdMS_TO_TICKS(sc.s360Duration));
          t35Servos[i].write(90); // stop
        }
      }
      return;
    }
  }
#endif
#ifdef BOARD_ESP32C3_21_1
  if (servoPin == PIN_FLEX_CH01 && servoFlex6Attached) {
    if (c3FlexConfig.gpio6Servo180) {
      LOG_INFO("Servo", String("Flex GPIO6 180°: ") + c3FlexConfig.gpio6S180Start + "° → " + c3FlexConfig.gpio6S180End + "°");
      sweepOut180(servo_flex6, PIN_FLEX_CH01, c3FlexConfig.gpio6S180Start, c3FlexConfig.gpio6S180End, c3FlexConfig.gpio6S180Duration, "pos_f6", "Flex servo 6");
    } else if (c3FlexConfig.gpio6Servo360) {
      LOG_INFO("Servo", String("Flex GPIO6 360°: speed ") + c3FlexConfig.gpio6S360Speed);
      servo_flex6.write(c3FlexConfig.gpio6S360Speed);
      if (c3FlexConfig.gpio6S360Duration > 0) {
        vTaskDelay(pdMS_TO_TICKS(c3FlexConfig.gpio6S360Duration));
        servo_flex6.write(90);
      }
    }
    return;
  }
  if (servoPin == PIN_FLEX_CH02 && servoFlex7Attached) {
    if (c3FlexConfig.gpio7Servo180) {
      LOG_INFO("Servo", String("Flex GPIO7 180°: ") + c3FlexConfig.gpio7S180Start + "° → " + c3FlexConfig.gpio7S180End + "°");
      sweepOut180(servo_flex7, PIN_FLEX_CH02, c3FlexConfig.gpio7S180Start, c3FlexConfig.gpio7S180End, c3FlexConfig.gpio7S180Duration, "pos_f7", "Flex servo 7");
    } else if (c3FlexConfig.gpio7Servo360) {
      LOG_INFO("Servo", String("Flex GPIO7 360°: speed ") + c3FlexConfig.gpio7S360Speed);
      servo_flex7.write(c3FlexConfig.gpio7S360Speed);
      if (c3FlexConfig.gpio7S360Duration > 0) {
        vTaskDelay(pdMS_TO_TICKS(c3FlexConfig.gpio7S360Duration));
        servo_flex7.write(90);
      }
    }
    return;
  }
#endif
#if ENABLE_DISPLAY
  if (servoPin == 13 && servo1Enabled && servoConfig.servo1Active()) {
#else
  if (servoPin == 12 && servo1Enabled && servoConfig.servo1Active()) {
#endif
    // Positional servo: sweep Start → End
    LOG_INFO("Servo", String("180° Servo: ") + String(servoConfig.servo1Start) + "° → " + String(servoConfig.servo1End) + "°");
    sweepOut180(servo1, servo1Pin(), servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration, "pos1", "180° Servo");
#if ENABLE_DISPLAY
  } else if (servoPin == 10 && servo2Attached && servoConfig.servo2Active()) {
#else
  } else if (servoPin == 12 && servo2Attached && servoConfig.servo2Active()) {
#endif
    // Continuous servo: spin at configured speed for configured duration
    LOG_INFO("Servo", String("360° Servo: spin at speed ") + String(servoConfig.servo2Speed) + " for " + String(servoConfig.servo2Duration) + "ms");
    servo2.write(servoConfig.servo2Speed);
    if (servoConfig.servo2Duration > 0) {
      vTaskDelay(pdMS_TO_TICKS(servoConfig.servo2Duration));
      servo2.write(90); // Stop
      LOG_INFO("Servo", "360° Servo: stopped");
    }
    // If duration == 0, servo keeps spinning until deactivateServo() is called
  }
}

void deactivateServo(int servoPin) {
#ifdef BOARD_JC3248W535C
  {
    int i = t35ServoIdx(servoPin);
    if (i >= 0 && t35Attached[i]) {
      const T35AmbientConfig::ServoChannel& sc = t35AmbientConfig.servo[i];
      if (sc.servo180) {
        LOG_INFO("Servo", String("T35 GPIO") + servoPin + " 180°: returning to " + sc.s180Start + "°");
        sweepBack180(t35Servos[i], T35_SERVO_GPIO[i], sc.s180Start, sc.s180End, sc.s180Duration, (String("pos_t") + i).c_str(), "T35 servo");
      } else if (sc.servo360) {
        t35Servos[i].write(90); // stop
        LOG_INFO("Servo", String("T35 GPIO") + servoPin + " 360°: stopped");
      }
      return;
    }
  }
#endif
#ifdef BOARD_ESP32C3_21_1
  if (servoPin == PIN_FLEX_CH01 && servoFlex6Attached && c3FlexConfig.gpio6Servo180) {
    LOG_INFO("Servo", String("Flex GPIO6 180°: returning to ") + c3FlexConfig.gpio6S180Start + "°");
    sweepBack180(servo_flex6, PIN_FLEX_CH01, c3FlexConfig.gpio6S180Start, c3FlexConfig.gpio6S180End, c3FlexConfig.gpio6S180Duration, "pos_f6", "Flex servo 6");
    return;
  }
  if (servoPin == PIN_FLEX_CH02 && servoFlex7Attached && c3FlexConfig.gpio7Servo180) {
    LOG_INFO("Servo", String("Flex GPIO7 180°: returning to ") + c3FlexConfig.gpio7S180Start + "°");
    sweepBack180(servo_flex7, PIN_FLEX_CH02, c3FlexConfig.gpio7S180Start, c3FlexConfig.gpio7S180End, c3FlexConfig.gpio7S180Duration, "pos_f7", "Flex servo 7");
    return;
  }
#endif
#if ENABLE_DISPLAY
  if (servoPin == 13 && servo1Enabled && servoConfig.servo1Active()) {
#else
  if (servoPin == 12 && servo1Enabled && servoConfig.servo1Active()) {
#endif
    // Positional servo: return End → Start
    LOG_INFO("Servo", String("180° Servo: returning to ") + String(servoConfig.servo1Start) + "°");
    sweepBack180(servo1, servo1Pin(), servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration, "pos1", "180° Servo");
#if ENABLE_DISPLAY
  } else if (servoPin == 10 && servo2Attached && servoConfig.servo2Active()) {
#else
  } else if (servoPin == 12 && servo2Attached && servoConfig.servo2Active()) {
#endif
    // Continuous servo: stop
    servo2.write(90);
    LOG_INFO("Servo", "360° Servo: stopped");
  }
}

void detachServos() {
  if (servo1Attached) { servo1.detach(); servo1Attached = false; }
  servo1Enabled = false;
  if (servo2Attached) { servo2.detach(); servo2Attached = false; }
#ifdef BOARD_ESP32C3_21_1
  if (servoFlex6Attached) { servo_flex6.detach(); servoFlex6Attached = false; }
  if (servoFlex7Attached) { servo_flex7.detach(); servoFlex7Attached = false; }
#endif
#ifdef BOARD_JC3248W535C
  for (int i = 0; i < 6; i++) {
    if (t35Attached[i]) { t35Servos[i].detach(); t35Attached[i] = false; }
  }
#endif
}
