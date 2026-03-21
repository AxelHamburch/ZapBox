#include "ServoControl.h"
#include "GlobalState.h"
#include "Log.h"

#if ENABLE_DISPLAY
#include <ESP32Servo.h>

static Servo servo1;  // Pin 13, paired with Relay 1 (Pin 12)
static Servo servo2;  // Pin 10, paired with Relay 2 (Pin 11)
static bool servo1Attached = false;
static bool servo2Attached = false;
// Toggle state: tracks current position for Return=No mode
static bool servo1AtEnd = false;
static bool servo2AtEnd = false;

// Slowly sweep a servo from one angle to another over the given duration.
// If duration_ms == 0, jump directly to target (native servo speed).
static void slowSweep(Servo &srv, int fromAngle, int toAngle, int duration_ms) {
  int steps = abs(toAngle - fromAngle);
  if (steps == 0) {
    srv.write(toAngle);
    return;
  }
  if (duration_ms <= 0) {
    // Jump directly - servo moves at its native speed
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
    servo1AtEnd = false;
    servo1.write(servoConfig.servo1Start);
    LOG_INFO("Servo", String("Servo 1 attached to Pin 13, start position: ") + String(servoConfig.servo1Start) + "°");
  }
  if (servoConfig.servo2Active()) {
    servo2.attach(10);
    servo2Attached = true;
    servo2AtEnd = false;
    servo2.write(servoConfig.servo2Start);
    LOG_INFO("Servo", String("Servo 2 attached to Pin 10, start position: ") + String(servoConfig.servo2Start) + "°");
  }
}

void activateServo(int relayPin) {
  if (relayPin == 12 && servo1Attached && servoConfig.servo1Active()) {
    if (servoConfig.returnToStart) {
      // Return mode: always drive Start → End
      LOG_INFO("Servo", String("Servo 1: ") + String(servoConfig.servo1Start) + "° → " + String(servoConfig.servo1End) + "°");
      slowSweep(servo1, servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration);
    } else {
      // Toggle mode: alternate direction each trigger
      if (!servo1AtEnd) {
        LOG_INFO("Servo", String("Servo 1 toggle: ") + String(servoConfig.servo1Start) + "° → " + String(servoConfig.servo1End) + "°");
        slowSweep(servo1, servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration);
        servo1AtEnd = true;
      } else {
        LOG_INFO("Servo", String("Servo 1 toggle: ") + String(servoConfig.servo1End) + "° → " + String(servoConfig.servo1Start) + "°");
        slowSweep(servo1, servoConfig.servo1End, servoConfig.servo1Start, servoConfig.servo1Duration);
        servo1AtEnd = false;
      }
    }
  } else if (relayPin == 11 && servo2Attached && servoConfig.servo2Active()) {
    if (servoConfig.returnToStart) {
      LOG_INFO("Servo", String("Servo 2: ") + String(servoConfig.servo2Start) + "° → " + String(servoConfig.servo2End) + "°");
      slowSweep(servo2, servoConfig.servo2Start, servoConfig.servo2End, servoConfig.servo2Duration);
    } else {
      if (!servo2AtEnd) {
        LOG_INFO("Servo", String("Servo 2 toggle: ") + String(servoConfig.servo2Start) + "° → " + String(servoConfig.servo2End) + "°");
        slowSweep(servo2, servoConfig.servo2Start, servoConfig.servo2End, servoConfig.servo2Duration);
        servo2AtEnd = true;
      } else {
        LOG_INFO("Servo", String("Servo 2 toggle: ") + String(servoConfig.servo2End) + "° → " + String(servoConfig.servo2Start) + "°");
        slowSweep(servo2, servoConfig.servo2End, servoConfig.servo2Start, servoConfig.servo2Duration);
        servo2AtEnd = false;
      }
    }
  }
}

void deactivateServo(int relayPin) {
  // In toggle mode (returnToStart=false) the servo stays where it is;
  // direction reverses on next activateServo() call.
  if (!servoConfig.returnToStart) return;
  // Return mode: drive back End → Start
  if (relayPin == 12 && servo1Attached && servoConfig.servo1Active()) {
    LOG_INFO("Servo", String("Servo 1: returning to ") + String(servoConfig.servo1Start) + "°");
    slowSweep(servo1, servoConfig.servo1End, servoConfig.servo1Start, servoConfig.servo1Duration);
  } else if (relayPin == 11 && servo2Attached && servoConfig.servo2Active()) {
    LOG_INFO("Servo", String("Servo 2: returning to ") + String(servoConfig.servo2Start) + "°");
    slowSweep(servo2, servoConfig.servo2End, servoConfig.servo2Start, servoConfig.servo2Duration);
  }
}

void detachServos() {
  if (servo1Attached) { servo1.detach(); servo1Attached = false; }
  if (servo2Attached) { servo2.detach(); servo2Attached = false; }
}

#else
// Headless build: no servo support (GPIO 10/11 are internal flash on ESP32-WROOM-32)
void initServos() {}
void activateServo(int relayPin) { (void)relayPin; }
void deactivateServo(int relayPin) { (void)relayPin; }
void detachServos() {}
#endif
