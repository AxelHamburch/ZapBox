#include "ServoControl.h"
#include "GlobalState.h"
#include "Log.h"

#if ENABLE_DISPLAY
#include <ESP32Servo.h>

static Servo servo1;  // Pin 13, positional 0-180°
static Servo servo2;  // Pin 10, continuous rotation 360°
static bool servo1Attached = false;
static bool servo2Attached = false;

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

void initServos() {
  if (servoConfig.servo1Active()) {
    servo1.attach(13);
    servo1Attached = true;
    servo1.write(servoConfig.servo1Start);
    LOG_INFO("Servo", String("Servo 1 (positional) attached to Pin 13, start: ") + String(servoConfig.servo1Start) + "°");
  }
  if (servoConfig.servo2Active()) {
    servo2.attach(10);
    servo2Attached = true;
    servo2.write(90); // 90 = stop for continuous servo
    LOG_INFO("Servo", String("Servo 2 (continuous) attached to Pin 10, stopped (90)"));
  }
}

void activateServo(int servoPin) {
  if (servoPin == 13 && servo1Attached && servoConfig.servo1Active()) {
    // Positional servo: sweep Start → End
    LOG_INFO("Servo", String("Servo 1: ") + String(servoConfig.servo1Start) + "° → " + String(servoConfig.servo1End) + "°");
    slowSweep(servo1, servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration);
  } else if (servoPin == 10 && servo2Attached && servoConfig.servo2Active()) {
    // Continuous servo: spin at configured speed for configured duration
    LOG_INFO("Servo", String("Servo 2: spin at speed ") + String(servoConfig.servo2Speed) + " for " + String(servoConfig.servo2Duration) + "ms");
    servo2.write(servoConfig.servo2Speed);
    if (servoConfig.servo2Duration > 0) {
      vTaskDelay(pdMS_TO_TICKS(servoConfig.servo2Duration));
      servo2.write(90); // Stop
      LOG_INFO("Servo", "Servo 2: stopped");
    }
    // If duration == 0, servo keeps spinning until deactivateServo() is called
  }
}

void deactivateServo(int servoPin) {
  if (servoPin == 13 && servo1Attached && servoConfig.servo1Active()) {
    // Positional servo: return End → Start
    LOG_INFO("Servo", String("Servo 1: returning to ") + String(servoConfig.servo1Start) + "°");
    slowSweep(servo1, servoConfig.servo1End, servoConfig.servo1Start, servoConfig.servo1Duration);
  } else if (servoPin == 10 && servo2Attached && servoConfig.servo2Active()) {
    // Continuous servo: stop
    servo2.write(90);
    LOG_INFO("Servo", "Servo 2: stopped");
  }
}

void detachServos() {
  if (servo1Attached) { servo1.detach(); servo1Attached = false; }
  if (servo2Attached) { servo2.detach(); servo2Attached = false; }
}

#else
// Headless build: no servo support
void initServos() {}
void activateServo(int servoPin) { (void)servoPin; }
void deactivateServo(int servoPin) { (void)servoPin; }
void detachServos() {}
#endif
