#include <assert.h>
#include <stdint.h>

#include "../examples/M5Core2CloudLogger/LoggerNumberParser.h"

namespace {

void expectValid(const char* text, uint32_t expected) {
  uint32_t actual = 123;
  const bool parsed = LoggerConfigFile::parseUnsignedDecimal(text, actual);
  assert(parsed);
  assert(actual == expected);
}

void expectInvalid(const char* text) {
  uint32_t actual = 123;
  const bool parsed = LoggerConfigFile::parseUnsignedDecimal(text, actual);
  assert(!parsed);
  assert(actual == 123);
}

}  // namespace

int main() {
  expectValid("0", 0);
  expectValid("60", 60);
  expectValid("4294967295", UINT32_MAX);
  expectInvalid(nullptr);
  expectInvalid("");
  expectInvalid("-1");
  expectInvalid("+1");
  expectInvalid(" 1");
  expectInvalid("1 ");
  expectInvalid("1.0");
  expectInvalid("4294967296");
  return 0;
}
