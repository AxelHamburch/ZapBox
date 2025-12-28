#ifndef PAYMENT_H
#define PAYMENT_H

#include <Arduino.h>

// Generate LNURL for given product pin using configured server/device
String generateLNURL(int pin);

// Update global lightning QR payload with given LNURL or URL
void updateLightningQR(const String& lnurlStr);

// Convenience: generate LNURL and update QR for given pin
void ensureQrForPin(int pin);

#endif // PAYMENT_H
