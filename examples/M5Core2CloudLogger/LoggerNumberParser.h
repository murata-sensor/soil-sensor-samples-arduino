#pragma once

#include <stdint.h>

namespace LoggerConfigFile {

inline bool parseUnsignedDecimal(const char* text, uint32_t& result) {
  if (text == nullptr || *text == '\0') return false;

  uint32_t parsed = 0;
  for (const char* current = text; *current != '\0'; ++current) {
    if (*current < '0' || *current > '9') return false;
    const uint32_t digit = static_cast<uint32_t>(*current - '0');
    if (parsed > (UINT32_MAX - digit) / 10U) return false;
    parsed = parsed * 10U + digit;
  }
  result = parsed;
  return true;
}

}  // namespace LoggerConfigFile
