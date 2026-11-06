#pragma once

#include "LoggerConfig.h"

class LoggerSerialConfig {
 public:
  bool active() const { return active_; }

  bool service(Stream& serial, uint32_t now, String& error) {
    if (active_ && static_cast<uint32_t>(now - beganAt_) >= 120000) {
      active_ = false;
      line_ = "";
      discarding_ = false;
      error = F("USB configuration timed out");
      serial.println(F("[CONFIG] ERROR timeout"));
    }
    if (line_.length() && static_cast<uint32_t>(now - lastByteAt_) >= 10000) {
      line_ = "";
      discarding_ = true;
      lastByteAt_ = now;
      error = F("USB configuration line timed out");
      serial.println(F("[CONFIG] ERROR line_timeout"));
    }
    for (size_t i = 0; i < 256 && serial.available(); ++i) {
      const int byte = serial.read();
      if (byte < 0) break;
      lastByteAt_ = now;
      if (byte == '\n') {
        if (discarding_) {
          discarding_ = false;
          line_ = "";
          continue;
        }
        if (line_.endsWith("\r")) line_.remove(line_.length() - 1);
        if (line_ == "CONFIG BEGIN") {
          active_ = true;
          beganAt_ = now;
          serial.println(F("[CONFIG] READY"));
        } else if (line_ == "CONFIG CANCEL") {
          active_ = false;
          serial.println(F("[CONFIG] CANCELLED"));
        } else if (line_.length()) {
          if (!active_) {
            error = F("USB configuration requires CONFIG BEGIN");
            serial.println(F("[CONFIG] ERROR expected_begin"));
          } else {
            LoggerConfig candidate;
            String message;
            if (!LoggerConfigNvs::parse(line_.c_str(), candidate, message) ||
                !LoggerConfigNvs::save(candidate, message)) {
              error = message;
              serial.print(F("[CONFIG] ERROR "));
              serial.println(message);
            } else {
              line_ = "";
              serial.println(F("[CONFIG] SAVED"));
              return true;
            }
          }
        }
        line_ = "";
      } else if (!discarding_) {
        const bool terminalCr =
            byte == '\r' && line_.length() == LoggerConfigNvs::kMaxJsonBytes;
        if (byte == 0 ||
            (line_.length() >= LoggerConfigNvs::kMaxJsonBytes && !terminalCr)) {
          line_ = "";
          discarding_ = true;
          error = F("USB configuration line too long or contains NUL");
          serial.println(F("[CONFIG] ERROR invalid_line"));
        } else {
          line_ += static_cast<char>(byte);
        }
      }
    }
    return false;
  }

 private:
  String line_;
  bool active_ = false;
  bool discarding_ = false;
  uint32_t beganAt_ = 0;
  uint32_t lastByteAt_ = 0;
};
