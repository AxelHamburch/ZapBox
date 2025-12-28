#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

// External button single-click behavior (wake/navigate)
void handleExternalSingleClick();

// Debounced external button edge detection and click sequencing
void handleExternalButton();

// Check long-hold actions (Help/Config) while button is pressed
void checkExternalButtonHolds();

// Exit config mode via button releases after guard period
void handleConfigExitButtons();

#endif // INPUT_H
