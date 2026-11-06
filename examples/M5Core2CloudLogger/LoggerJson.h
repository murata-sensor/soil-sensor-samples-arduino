#pragma once

#include <ArduinoJson.h>

namespace LoggerJson {

struct Input {
  const char* next;
  int read() { return *next ? static_cast<unsigned char>(*next++) : -1; }
  size_t readBytes(char* buffer, size_t length) {
    size_t count = 0;
    while (count < length && *next) buffer[count++] = *next++;
    return count;
  }
};

inline bool parseObject(const char* json, JsonDocument& document) {
  Input input{json};
  const DeserializationError result = deserializeJson(document, input);
  // ArduinoJson accepts a valid prefix; require the entire object input.
  while (*input.next == ' ' || *input.next == '\t' || *input.next == '\r' ||
         *input.next == '\n')
    ++input.next;
  return !result && document.is<JsonObject>() && !*input.next;
}

}  // namespace LoggerJson
