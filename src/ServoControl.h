#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

/**
 * ServoControl.h - Servo motor control for Servo multi-channel mode
 *
 * Pin assignment (each channel independent):
 *   Pin 12 - Relay 1
 *   Pin 13 - Servo 1 (positional 0-180°, PWM via LEDC)
 *   Pin 10 - Servo 2 (continuous rotation 360°, PWM via LEDC)
 *   Pin 11 - Relay 2
 *
 * Servos are triggered independently by their own pin from the server.
 */

/**
 * Initialize servo motors based on ServoConfig.
 * Attaches active servos and moves them to their start/stop position.
 */
void initServos();

/**
 * Activate the servo on the given pin (13 or 10).
 * Servo 1 (pin 13): sweeps Start→End angle.
 * Servo 2 (pin 10): spins at configured speed for configured duration.
 * @param servoPin  GPIO of the servo being triggered (13 or 10)
 */
void activateServo(int servoPin);

/**
 * Return the servo on the given pin to its rest state.
 * Servo 1 (pin 13): sweeps End→Start angle.
 * Servo 2 (pin 10): stops (write 90).
 * @param servoPin  GPIO of the servo being deactivated (13 or 10)
 */
void deactivateServo(int servoPin);

/**
 * Detach all servos (frees LEDC channels, stops PWM signal).
 */
void detachServos();

#endif // SERVO_CONTROL_H
