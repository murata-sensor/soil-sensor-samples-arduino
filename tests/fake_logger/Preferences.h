#pragma once

#include <Arduino.h>

#include <map>

class Preferences {
 public:
  struct State {
    std::map<std::string, String> values;
    bool openFails = false;
    bool writeFails = false;
    size_t writes = 0;
  };
  static State& state() {
    static State value;
    return value;
  }
  bool begin(const char*, bool) { return !state().openFails; }
  void end() {}
  bool isKey(const char* key) { return state().values.count(key) != 0; }
  String getString(const char* key) {
    const auto found = state().values.find(key);
    return found == state().values.end() ? String() : found->second;
  }
  size_t putString(const char* key, const char* value) {
    ++state().writes;
    if (state().writeFails) return 0;
    state().values[key] = value;
    return std::strlen(value);
  }
};
