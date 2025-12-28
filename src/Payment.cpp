#include <Arduino.h>
#include "GlobalState.h"
#include "Payment.h"
#include "Log.h"
#include <vector>

// Access configuration provided in main.cpp
extern String lnbitsServer;
extern String deviceId;
extern String qrFormat; // "bech32" or "lud17"

// Bech32 encoding helpers (copied from main.cpp to isolate payment logic)
static uint32_t bech32Polymod(const std::vector<uint8_t>& values) {
  uint32_t chk = 1;
  for (size_t i = 0; i < values.size(); ++i) {
    uint8_t top = chk >> 25;
    chk = (chk & 0x1ffffff) << 5 ^ values[i];
    if (top & 1) chk ^= 0x3b6a57b2;
    if (top & 2) chk ^= 0x26508e6d;
    if (top & 4) chk ^= 0x1ea119fa;
    if (top & 8) chk ^= 0x3d4233dd;
    if (top & 16) chk ^= 0x2a1462b3;
  }
  return chk;
}

static std::vector<uint8_t> bech32HrpExpand(const String& hrp) {
  std::vector<uint8_t> ret;
  ret.reserve(hrp.length() * 2 + 1);
  for (size_t i = 0; i < hrp.length(); ++i) {
    ret.push_back(hrp[i] >> 5);
  }
  ret.push_back(0);
  for (size_t i = 0; i < hrp.length(); ++i) {
    ret.push_back(hrp[i] & 31);
  }
  return ret;
}

static std::vector<uint8_t> convertBits(const uint8_t* data, size_t len, int frombits, int tobits, bool pad) {
  std::vector<uint8_t> ret;
  int acc = 0;
  int bits = 0;
  int maxv = (1 << tobits) - 1;
  for (size_t i = 0; i < len; i++) {
    int value = data[i];
    acc = (acc << frombits) | value;
    bits += frombits;
    while (bits >= tobits) {
      bits -= tobits;
      ret.push_back((acc >> bits) & maxv);
    }
  }
  if (pad) {
    if (bits > 0) {
      ret.push_back((acc << (tobits - bits)) & maxv);
    }
  } else if (bits >= frombits || ((acc << (tobits - bits)) & maxv)) {
    return std::vector<uint8_t>();
  }
  return ret;
}

static String encodeBech32(const String& data) {
  String hrp = "lnurl";
  // Convert data to bytes (keep original case)
  std::vector<uint8_t> dataBytes;
  for (size_t i = 0; i < data.length(); i++) {
    dataBytes.push_back((uint8_t)data[i]);
  }
  // Convert from 8-bit to 5-bit
  std::vector<uint8_t> data5bit = convertBits(dataBytes.data(), dataBytes.size(), 8, 5, true);
  if (data5bit.empty()) {
    return "";
  }
  // Calculate checksum
  std::vector<uint8_t> combined = bech32HrpExpand(hrp);
  combined.insert(combined.end(), data5bit.begin(), data5bit.end());
  combined.insert(combined.end(), 6, 0);
  uint32_t polymod = bech32Polymod(combined) ^ 1;  // XOR with 1 for Bech32 (not Bech32m)
  std::vector<uint8_t> checksum;
  for (int i = 0; i < 6; ++i) {
    checksum.push_back((polymod >> (5 * (5 - i))) & 31);
  }
  // Build final string
  String result = hrp + "1";
  for (size_t i = 0; i < data5bit.size(); i++) {
    result += BECH32_CHARSET[data5bit[i]];
  }
  for (size_t i = 0; i < checksum.size(); i++) {
    result += BECH32_CHARSET[checksum[i]];
  }
  // Convert to uppercase for LNURL standard
  result.toUpperCase();
  return result;
}

// Generate LNURL for a given pin
String generateLNURL(int pin) {
  if (lnbitsServer.length() == 0 || deviceId.length() == 0) {
    LOG_WARN("LNURL", "Cannot generate - server or deviceId not configured");
    return "";
  }
  // Build URL: https://{server}/bitcoinswitch/api/v1/lnurl/{deviceId}?pin={pin}
  String url = "https://" + lnbitsServer + "/bitcoinswitch/api/v1/lnurl/" + deviceId + "?pin=" + String(pin);
  LOG_DEBUG("LNURL", String("Generated for pin ") + String(pin) + String(": ") + url);
  if (qrFormat == "lud17") {
    // LUD17 format: replace https: with lnurlp:
    String result = url;
    result.replace("https:", "lnurlp:");
    LOG_DEBUG("LNURL", String("LUD17 format: ") + result);
    return result;
  } else {
    // BECH32 format (default)
    // Encode the https URL with bech32 and prepend lightning:
    String encoded = encodeBech32(url);
    if (encoded.length() == 0) {
      LOG_WARN("LNURL", "BECH32 encoding failed, falling back to URL format");
      return String(wifiConfig.lightningPrefix) + url;
    }
    String result = String(wifiConfig.lightningPrefix) + encoded;
    LOG_DEBUG("LNURL", String("BECH32 format: ") + result);
    return result;
  }
}

// Update lightning QR with given LNURL
void updateLightningQR(const String& lnurlStr) {
  String tmp = lnurlStr;
  tmp.trim();
  // Don't convert to uppercase - keep original case (important for LUD17)
  if (tmp.startsWith("lightning:") || tmp.startsWith("LIGHTNING:")) {
    // Already has prefix, use as-is
    strncpy(lightningConfig.lightning, tmp.c_str(), sizeof(lightningConfig.lightning) - 1);
    lightningConfig.lightning[sizeof(lightningConfig.lightning) - 1] = '\0';
  } else {
    // No prefix, add it (lowercase for consistency)
    strncpy(lightningConfig.lightning, wifiConfig.lightningPrefix, sizeof(lightningConfig.lightning) - 1);
    strncat(lightningConfig.lightning, tmp.c_str(), sizeof(lightningConfig.lightning) - 1 - strlen(lightningConfig.lightning));
  }
  LOG_DEBUG("QR", String("Updated lightning QR: ") + lightningConfig.lightning);
}

// Convenience wrapper: Generate LNURL and update QR for given pin
void ensureQrForPin(int pin) {
  String lnurlStr = generateLNURL(pin);
  if (lnurlStr.length() > 0) {
    updateLightningQR(lnurlStr);
  } else {
    LOG_WARN("LNURL", String("Skipping QR update for pin ") + String(pin) + String(" (empty LNURL)"));
  }
}
