#ifndef TEST_FAKE_ARDUINO_H
#define TEST_FAKE_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

extern uint32_t fakeArduinoMillis;

class __FlashStringHelper;
#ifndef F
#define F(text) (reinterpret_cast<const __FlashStringHelper*>(text))
#endif

inline uint32_t millis() { return fakeArduinoMillis; }
inline void delay(uint32_t milliseconds) { fakeArduinoMillis += milliseconds; }

class Stream {
 public:
  virtual ~Stream() {}
  virtual int available() = 0;
  virtual int read() = 0;
  virtual size_t write(const uint8_t* data, size_t size) = 0;
  virtual void flush() {}
};

#endif
