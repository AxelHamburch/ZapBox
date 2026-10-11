#ifndef NAVIGATION_H
#define NAVIGATION_H

/**
 * Navigation.h - Touch and Product Navigation Module
 * 
 * This module handles:
 * - Touch button interaction and multi-click detection
 * - Product navigation in multi-channel mode
 * - Mode transitions (Help, Report, Config)
 */

/**
 * Handle touch button interactions including:
 * - Multi-click detection (1-4 clicks)
 * - Mode transitions (Help after 1 click, Report after 2, Config after 4)
 * - Screensaver wake-up
 * - Config mode exit via touch
 */
void handleTouchButton();

/**
 * Navigate to next product in multi-channel mode.
 * Handles product cycling, ticker display, and LNURL generation.
 * Wakes device from power saving mode if needed.
 */
void navigateToNextProduct();
bool hideCh01Browsing(); // true when CH01 is skipped while browsing (T35 "Display standard output" = No)



#endif // NAVIGATION_H
