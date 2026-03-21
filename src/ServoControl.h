#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

/**
 * ServoControl.h - Servo motor control for Servo multi-channel mode
 *
 * Pin assignment:
 *   Pin 12 - Relay 1 (paired with Servo 1)
 *   Pin 13 - Servo 1 (PWM via LEDC)
 *   Pin 10 - Servo 2 (PWM via LEDC)
 *   Pin 11 - Relay 2 (paired with Servo 2)
 *
 * Only compiled/active when multiChannelConfig.mode == "servo".
 */

/**
 * Initialize servo motors based on ServoConfig.
 * Attaches active servos and moves them to their start position.
 */
void initServos();

/**
 * Activate servo paired with the given relay pin.
 * Moves servo from start angle to end angle.
 * @param relayPin  GPIO of the relay being triggered (12 or 11)
 */
void activateServo(int relayPin);

/**
 * Return servo paired with the given relay pin to start position.
 * Only acts if servoConfig.returnToStart is true.
 * @param relayPin  GPIO of the relay being deactivated (12 or 11)
 */
void deactivateServo(int relayPin);

/**
 * Detach all servos (frees LEDC channels, stops PWM signal).
 */
void detachServos();

#endif // SERVO_CONTROL_H
