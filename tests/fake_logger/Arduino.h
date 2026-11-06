#pragma once

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#define F(text) text

class String : public std::string {
 public:
  using std::string::string;
  using std::string::operator=;
  using std::string::operator+=;
  String() = default;
  String(const std::string& value) : std::string(value) {}
  explicit String(char value) : std::string(1, value) {}
  String(float value, unsigned int decimals) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(decimals) << value;
    assign(output.str());
  }
  String& operator+=(uint32_t value) {
    append(std::to_string(value));
    return *this;
  }
  String& operator+=(int32_t value) {
    append(std::to_string(value));
    return *this;
  }
  bool isEmpty() const { return empty(); }
  bool startsWith(const char* text) const { return find(text) == 0; }
  bool endsWith(const char* text) const {
    const size_t size = std::strlen(text);
    return length() >= size && compare(length() - size, size, text) == 0;
  }
  void remove(size_t index) { erase(index); }
  void trim() {
    const size_t first = find_first_not_of(" \t\r\n");
    if (first == npos) {
      clear();
      return;
    }
    erase(find_last_not_of(" \t\r\n") + 1);
    erase(0, first);
  }
  int indexOf(char value) const {
    const size_t index = find(value);
    return index == npos ? -1 : static_cast<int>(index);
  }
  int lastIndexOf(char value) const {
    const size_t index = rfind(value);
    return index == npos ? -1 : static_cast<int>(index);
  }
  String substring(size_t start, size_t end = npos) const {
    const std::string value = substr(start, end == npos ? npos : end - start);
    return String(value.c_str());
  }
};

class Stream {
 public:
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual size_t write(uint8_t byte) = 0;
  void print(const char* text) {
    while (*text) write(static_cast<uint8_t>(*text++));
  }
  void println(const char* text) {
    print(text);
    print("\r\n");
  }
  void println(const String& text) { println(text.c_str()); }
};
