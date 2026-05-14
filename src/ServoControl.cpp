#include "ServoControl.h"
#include "GlobalState.h"
#include "PinConfig.h"
#include "Log.h"
#include <ESP32Servo.h>

static Servo servo1;  // Display: Pin 13 positional | Headless: Pin 12 (servo180)
static Servo servo2;  // Display: Pin 10 continuous  | Headless: Pin 12 (servo360)
static bool servo1Attached = false;
static bool servo2Attached = false;

#ifdef BOARD_ESP32C3_21_1
static Servo servo_flex6;  // C3 GPIO6 flex channel
static Servo servo_flex7;  // C3 GPIO7 flex channel
static bool servoFlex6Attached = false;
static bool servoFlex7Attached = false;
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

void initServos() {
#if ENABLE_DISPLAY
  // Display version: servo1 on Pin 13, servo2 on Pin 10
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
#else
  // Headless version: servo on Pin 12 (single-pin mode)
  if (multiChannelConfig.mode == "servo180" && servoConfig.servo1Active()) {
    servo1.attach(12);
    servo1Attached = true;
    servo1.write(servoConfig.servo1Start);
    LOG_INFO("Servo", String("180° Servo attached to Pin 12, start: ") + String(servoConfig.servo1Start) + "°");
  } else if (multiChannelConfig.mode == "servo360" && servoConfig.servo2Active()) {
    servo2.attach(12);
    servo2Attached = true;
    servo2.write(90); // 90 = stop for continuous servo
    LOG_INFO("Servo", "360° Servo attached to Pin 12, stopped (90)");
  }
#endif
#ifdef BOARD_ESP32C3_21_1
  if (c3FlexConfig.gpio6Servo180 || c3FlexConfig.gpio6Servo360) {
    servo_flex6.attach(PIN_FLEX_CH01);
    servoFlex6Attached = true;
    servo_flex6.write(c3FlexConfig.gpio6Servo180 ? c3FlexConfig.gpio6S180Start : 90);
    LOG_INFO("Servo", String("Flex GPIO6 servo attached, start: ") +
             (c3FlexConfig.gpio6Servo180 ? String(c3FlexConfig.gpio6S180Start) + "°" : String("90 (stop)")));
  }
  if (c3FlexConfig.gpio7Servo180 || c3FlexConfig.gpio7Servo360) {
    servo_flex7.attach(PIN_FLEX_CH02);
    servoFlex7Attached = true;
    servo_flex7.write(c3FlexConfig.gpio7Servo180 ? c3FlexConfig.gpio7S180Start : 90);
    LOG_INFO("Servo", String("Flex GPIO7 servo attached, start: ") +
             (c3FlexConfig.gpio7Servo180 ? String(c3FlexConfig.gpio7S180Start) + "°" : String("90 (stop)")));
  }
#endif
}

void activateServo(int servoPin) {
#ifdef BOARD_ESP32C3_21_1
  if (servoPin == PIN_FLEX_CH01 && servoFlex6Attached) {
    if (c3FlexConfig.gpio6Servo180) {
      LOG_INFO("Servo", String("Flex GPIO6 180°: ") + c3FlexConfig.gpio6S180Start + "° → " + c3FlexConfig.gpio6S180End + "°");
      slowSweep(servo_flex6, c3FlexConfig.gpio6S180Start, c3FlexConfig.gpio6S180End, c3FlexConfig.gpio6S180Duration);
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
      slowSweep(servo_flex7, c3FlexConfig.gpio7S180Start, c3FlexConfig.gpio7S180End, c3FlexConfig.gpio7S180Duration);
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
  if (servoPin == 13 && servo1Attached && servoConfig.servo1Active()) {
#else
  if (servoPin == 12 && servo1Attached && servoConfig.servo1Active()) {
#endif
    // Positional servo: sweep Start → End
    LOG_INFO("Servo", String("180° Servo: ") + String(servoConfig.servo1Start) + "° → " + String(servoConfig.servo1End) + "°");
    slowSweep(servo1, servoConfig.servo1Start, servoConfig.servo1End, servoConfig.servo1Duration);
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
#ifdef BOARD_ESP32C3_21_1
  if (servoPin == PIN_FLEX_CH01 && servoFlex6Attached && c3FlexConfig.gpio6Servo180) {
    LOG_INFO("Servo", String("Flex GPIO6 180°: returning to ") + c3FlexConfig.gpio6S180Start + "°");
    slowSweep(servo_flex6, c3FlexConfig.gpio6S180End, c3FlexConfig.gpio6S180Start, c3FlexConfig.gpio6S180Duration);
    return;
  }
  if (servoPin == PIN_FLEX_CH02 && servoFlex7Attached && c3FlexConfig.gpio7Servo180) {
    LOG_INFO("Servo", String("Flex GPIO7 180°: returning to ") + c3FlexConfig.gpio7S180Start + "°");
    slowSweep(servo_flex7, c3FlexConfig.gpio7S180End, c3FlexConfig.gpio7S180Start, c3FlexConfig.gpio7S180Duration);
    return;
  }
#endif
#if ENABLE_DISPLAY
  if (servoPin == 13 && servo1Attached && servoConfig.servo1Active()) {
#else
  if (servoPin == 12 && servo1Attached && servoConfig.servo1Active()) {
#endif
    // Positional servo: return End → Start
    LOG_INFO("Servo", String("180° Servo: returning to ") + String(servoConfig.servo1Start) + "°");
    slowSweep(servo1, servoConfig.servo1End, servoConfig.servo1Start, servoConfig.servo1Duration);
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
  if (servo2Attached) { servo2.detach(); servo2Attached = false; }
#ifdef BOARD_ESP32C3_21_1
  if (servoFlex6Attached) { servo_flex6.detach(); servoFlex6Attached = false; }
  if (servoFlex7Attached) { servo_flex7.detach(); servoFlex7Attached = false; }
#endif
}
