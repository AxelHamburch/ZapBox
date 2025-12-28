#ifndef LOG_HELPER_H
#define LOG_HELPER_H

#include <Arduino.h>

namespace Log {
  enum Level {
    ERROR = 0,
    WARN  = 1,
    INFO  = 2,
    DEBUG = 3
  };

#ifndef LOG_ENABLE
#define LOG_ENABLE 1
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL Log::INFO
#endif

  inline const char* levelName(Level lvl) {
    switch (lvl) {
      case ERROR: return "ERROR";
      case WARN:  return "WARN";
      case INFO:  return "INFO";
      case DEBUG: return "DEBUG";
      default:    return "INFO";
    }
  }

  // Core print function
  inline void print(Level lvl, const char* tag, const String& message, bool showTime = true) {
#if LOG_ENABLE
    if ((int)lvl <= (int)LOG_LEVEL) {
      String out;
      out += "[";
      out += levelName(lvl);
      out += "]";
      if (tag && *tag) {
        out += "[";
        out += tag;
        out += "] ";
      } else {
        out += " ";
      }
      if (showTime) {
        out += "[+";
        out += String(millis() / 1000.0, 1);
        out += "s] ";
      }
      out += message;
      Serial.println(out);
    }
#endif
  }
}

// Convenience macros
#define LOG_ERROR(TAG, MSG) Log::print(Log::ERROR, TAG, String(MSG))
#define LOG_WARN(TAG, MSG)  Log::print(Log::WARN,  TAG, String(MSG))
#define LOG_INFO(TAG, MSG)  Log::print(Log::INFO,  TAG, String(MSG))
#define LOG_DEBUG(TAG, MSG) Log::print(Log::DEBUG, TAG, String(MSG))

#endif // LOG_HELPER_H
