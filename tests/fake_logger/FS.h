#pragma once

#include <Arduino.h>

#include <algorithm>
#include <map>
#include <vector>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {
class FS;
}

class File {
 public:
  std::string text;
  bool valid = false;
  bool directory = false;
  bool shortRead = false;
  size_t writeLimit = std::string::npos;
  size_t failWriteAt = 0;
  size_t writes = 0;
  size_t reads = 0;
  size_t flushes = 0;
  explicit operator bool() const { return valid; }
  bool isDirectory() const { return directory; }
  size_t size() const { return text.size(); }
  size_t readBytes(char* buffer, size_t size) {
    size_t count = std::min(size, text.size());
    if (shortRead && count) --count;
    std::memcpy(buffer, text.data(), count);
    return count;
  }
  String readString() {
    ++(storage_ ? storage_->reads : reads);
    const size_t count =
        shortRead && !text.empty() ? text.size() - 1 : text.size();
    return String(text.substr(0, count));
  }
  const char* name() const { return name_.c_str(); }
  File openNextFile() {
    return next_ < entries_.size() ? entries_[next_++] : File();
  }
  size_t print(const char* value) {
    return writeText(value, std::strlen(value));
  }
  size_t print(const String& value) {
    return writeText(value.data(), value.size());
  }
  size_t println(const char* value) { return println(String(value)); }
  size_t println(const String& value) { return print(String(value + "\r\n")); }
  void flush() { ++(storage_ ? storage_->flushes : flushes); }
  void close() {}

 private:
  friend class fs::FS;
  File* storage_ = nullptr;
  std::string name_;
  std::vector<File> entries_;
  size_t next_ = 0;

  size_t writeText(const char* value, size_t size) {
    File& target = storage_ ? *storage_ : *this;
    ++target.writes;
    size_t count = std::min(size, target.writeLimit);
    if (target.failWriteAt == target.writes && count) --count;
    target.text.append(value, count);
    if (storage_) text = target.text;
    return count;
  }
};

namespace fs {
class FS {
 public:
  std::map<std::string, File> files;
  size_t opens = 0;
  size_t checks = 0;
  bool openFails = false;
  bool mkdirFails = false;
  bool removeFails = false;
  bool renameFails = false;
  size_t writeLimit = std::string::npos;
  size_t failWriteAt = 0;
  bool exists(const char* path) {
    ++checks;
    return files.count(path) != 0;
  }
  bool exists(const String& path) { return exists(path.c_str()); }
  File open(const char* path, const char* mode = FILE_READ) {
    ++opens;
    if (openFails) return File();
    auto found = files.find(path);
    const bool writing = std::strcmp(mode, FILE_READ) != 0;
    if (found == files.end()) {
      if (!writing) return File();
      File file;
      file.valid = true;
      found = files.emplace(path, file).first;
    }
    if (!found->second.valid) return File();
    if (writing) {
      if (std::strcmp(mode, FILE_WRITE) == 0) found->second.text.clear();
      found->second.writeLimit = writeLimit;
      found->second.failWriteAt = failWriteAt;
      found->second.writes = 0;
    }
    File file = found->second;
    file.storage_ = &found->second;
    file.name_ = path;
    if (file.directory) {
      const std::string prefix = std::string(path) + '/';
      for (auto& item : files) {
        if (item.first.find(prefix) != 0 ||
            item.first.find('/', prefix.size()) != std::string::npos)
          continue;
        File entry = item.second;
        entry.storage_ = &item.second;
        entry.name_ = item.first;
        file.entries_.push_back(entry);
      }
    }
    return file;
  }
  File open(const String& path, const char* mode = FILE_READ) {
    return open(path.c_str(), mode);
  }
  bool mkdir(const char* path) {
    if (mkdirFails || files.count(path)) return false;
    File file;
    file.valid = file.directory = true;
    files[path] = file;
    return true;
  }
  bool remove(const String& path) {
    return !removeFails && files.erase(path) != 0;
  }
  bool rename(const String& source, const String& destination) {
    const auto found = files.find(source);
    if (renameFails || found == files.end() || files.count(destination))
      return false;
    files[destination] = found->second;
    files.erase(found);
    return true;
  }
};
}  // namespace fs
