#ifndef PAYMENT_H
#define PAYMENT_H

#include <Arduino.h>

// Generate LNURL for given product pin using configured server/device
String generateLNURL(int pin);

// Update global lightning QR payload with given LNURL or URL
void updateLightningQR(const String& lnurlStr);

#endif // PAYMENT_H
