#pragma once

#include <FS.h>

#include "LoggerConfig.h"
#include "LoggerNumberParser.h"

namespace LoggerConfigFile {

constexpr const char* kPath = "/slt5006.cfg";
constexpr size_t kMaxFileBytes = 4096;

inline bool parse(const char* text, LoggerConfig& config, String& message) {
  JsonDocument document;
  document.to<JsonObject>();
  if (!strncmp(text, "\xef\xbb\xbf", 3)) text += 3;
  while (*text) {
    String line;
    while (*text && *text != '\n') line += *text++;
    if (*text) ++text;
    line.trim();
    if (!line.length() || line[0] == '#' || line[0] == ';') continue;
    const int separator = line.indexOf('=');
    if (separator <= 0) {
      message = F("SD configuration requires key=value lines");
      return false;
    }
    String key = line.substring(0, separator);
    key.trim();
    String value = line.substring(separator + 1);
    value.trim();
    if (value.length() >= 2 &&
        ((value[0] == '"' && value[value.length() - 1] == '"') ||
         (value[0] == '\'' && value[value.length() - 1] == '\''))) {
      value = value.substring(1, value.length() - 1);
    }
    if (key == "sample_interval_sec" || key == "upload_batch_size" ||
        key == "deep_sleep_sec") {
      uint32_t number = 0;
      if (!parseUnsignedDecimal(value.c_str(), number)) {
        message = F("SD configuration requires unsigned decimal integers");
        return false;
      }
      document[key.c_str()] = number;
    } else {
      document[key.c_str()] = value.c_str();
    }
  }
  const size_t size = measureJson(document);
  if (document.overflowed() || size > LoggerConfigNvs::kMaxJsonBytes) {
    message = F("SD configuration exceeds supported size");
    return false;
  }
  char json[LoggerConfigNvs::kMaxJsonBytes + 1];
  serializeJson(document, json, sizeof(json));
  return LoggerConfigNvs::parse(json, config, message);
}

inline LoggerConfigNvs::LoadResult load(fs::FS* fs, LoggerConfig& config,
                                        String& message) {
  using LoggerConfigNvs::LoadResult;
  if (!fs || !fs->exists(kPath)) {
    message = F("No configuration; using offline defaults");
    return LoadResult::Missing;
  }
  File file = fs->open(kPath, FILE_READ);
  if (!file || file.isDirectory()) {
    message = F("SD configuration open failed");
    return LoadResult::Error;
  }
  const size_t size = file.size();
  if (size > kMaxFileBytes) {
    file.close();
    message = F("SD configuration exceeds 4096 bytes");
    return LoadResult::Error;
  }
  char text[kMaxFileBytes + 1];
  const size_t read = file.readBytes(text, size);
  file.close();
  text[read] = '\0';
  if (read != size || strlen(text) != size) {
    message = F("SD configuration read failed or contains NUL");
    return LoadResult::Error;
  }
  if (!parse(text, config, message)) return LoadResult::Error;
  message = config.cloudReady() ? F("SD configuration loaded")
                                : F("SD loaded; cloud disabled");
  return LoadResult::Loaded;
}

inline LoggerConfigNvs::LoadResult loadFallback(
    LoggerConfigNvs::LoadResult nvsResult, fs::FS* fs, LoggerConfig& config,
    String& message) {
  // An existing NVS config is authoritative, including offline defaults.
  // Storage errors are not absence and must not switch credentials silently.
  if (nvsResult != LoggerConfigNvs::LoadResult::Missing) return nvsResult;
  return load(fs, config, message);
}

}  // namespace LoggerConfigFile
