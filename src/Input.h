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

// Callback for NEXT button click to exit config mode
void onNextButtonConfigExit();

// Wrapper for NEXT button click - handles both navigation and config exit
void onNextButtonClick();

#endif // INPUT_H
