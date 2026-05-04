#pragma once

/**
 * IOExpander.h ÔÇö PCF8574 I2C GPIO expander abstraction
 *
 * Adds CH05ÔÇôCH12 to T-Display-S3 via a PCF8574 at I2C address 0x20.
 * The PCF8574 shares the existing I2C bus (SDA=GPIO18, SCL=GPIO17) with
 * the touch controller and the PN532 NFC reader.
 *
 * Address 0x20 is safe: PN532 uses 0x24, touch CST816S uses 0x15.
 * Wire.begin() must have been called before initIOExpander().
 *
 * Channel mapping:
 *   CH05 ÔåÆ PCF8574 P0
 *   CH06 ÔåÆ PCF8574 P1
 *   ...
 *   CH12 ÔåÆ PCF8574 P7
 */

/**
 * Initialize the PCF8574.
 * Must be called after touch.begin() (which calls Wire.begin()).
 * Sets all output pins LOW, configures sensor pins as inputs (HIGH).
 * Does nothing if ioExpanderConfig.enabled is false.
 */
void initIOExpander();

/**
 * Activate a relay channel on the PCF8574.
 * @param ch  Channel index 0ÔÇô7 (CH05=0 ÔÇª CH12=7)
 */
void activateExpanderChannel(int ch);

/**
 * Deactivate a relay channel on the PCF8574.
 * @param ch  Channel index 0ÔÇô7 (CH05=0 ÔÇª CH12=7)
 */
void deactivateExpanderChannel(int ch);

/**
 * Read a sensor pin from the PCF8574.
 * Returns true when the sensor pulls the pin LOW (active LOW / NPN input).
 * @param ch  Channel index 0ÔÇô7 (CH05=0 ÔÇª CH12=7)
 */
bool readExpanderSensor(int ch);
