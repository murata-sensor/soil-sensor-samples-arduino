#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "LoggerJson.h"

struct LoggerConfig {
  String wifiSsid;
  String wifiPassword;
  String gasUrl;
  String gasToken;
  String ntpServer = "pool.ntp.org";
  String timezone = "JST-9";
  uint32_t sampleIntervalSec = 60;
  uint8_t uploadBatchSize = 10;
  uint32_t deepSleepSec = 0;

  bool wifiReady() const { return wifiSsid.length(); }

  bool cloudReady() const {
    return wifiReady() && gasUrl.startsWith("https://") && gasToken.length();
  }
};

namespace LoggerConfigNvs {

constexpr size_t kMaxJsonBytes = 2048;
constexpr const char* kNamespace = "slt5006";
constexpr const char* kKey = "config-v1";
enum class LoadResult { Loaded, Missing, Error };

inline bool readString(JsonVariantConst value, String& target, size_t maximum) {
  if (!value.is<const char*>()) return false;
  const JsonString text = value.as<JsonString>();
  if (text.size() > maximum || strlen(text.c_str()) != text.size())
    return false;
  target = text.c_str();
  return true;
}

inline bool readNumber(JsonVariantConst value, uint32_t& target,
                       uint32_t minimum, uint32_t maximum) {
  if (!value.is<uint32_t>()) return false;
  const uint32_t number = value.as<uint32_t>();
  if (number < minimum || number > maximum) return false;
  target = number;
  return true;
}

inline bool parse(const char* json, LoggerConfig& config, String& message) {
  if (strlen(json) > kMaxJsonBytes) {
    message = F("Configuration exceeds 2048 bytes");
    return false;
  }
  JsonDocument document;
  if (!LoggerJson::parseObject(json, document)) {
    message = F("Configuration must be a JSON object");
    return false;
  }
  // Every upload replaces the complete configuration; omitted keys use
  // defaults.
  LoggerConfig candidate;
  for (JsonPairConst entry : document.as<JsonObjectConst>()) {
    const char* key = entry.key().c_str();
    const JsonVariantConst value = entry.value();
    bool valid = false;
    if (!strcmp(key, "wifi_ssid"))
      valid = readString(value, candidate.wifiSsid, 32);
    else if (!strcmp(key, "wifi_password"))
      valid = readString(value, candidate.wifiPassword, 64);
    else if (!strcmp(key, "gas_url"))
      valid = readString(value, candidate.gasUrl, 512);
    else if (!strcmp(key, "gas_token"))
      valid = readString(value, candidate.gasToken, 256);
    else if (!strcmp(key, "ntp_server"))
      valid = readString(value, candidate.ntpServer, 253) &&
              candidate.ntpServer.length();
    else if (!strcmp(key, "timezone"))
      valid = readString(value, candidate.timezone, 128) &&
              candidate.timezone.length();
    else if (!strcmp(key, "sample_interval_sec"))
      valid = readNumber(value, candidate.sampleIntervalSec, 5, 86400);
    else if (!strcmp(key, "upload_batch_size")) {
      uint32_t number = 0;
      valid = readNumber(value, number, 1, 20);
      if (valid) candidate.uploadBatchSize = static_cast<uint8_t>(number);
    } else if (!strcmp(key, "deep_sleep_sec")) {
      valid = readNumber(value, candidate.deepSleepSec, 0, 86400) &&
              (candidate.deepSleepSec == 0 || candidate.deepSleepSec >= 10);
    }
    if (!valid) {
      // Do not echo unknown keys or values: either may contain a secret.
      message = F("Unknown key or invalid configuration value");
      return false;
    }
  }
  if ((!candidate.gasUrl.isEmpty() &&
       !candidate.gasUrl.startsWith("https://")) ||
      (candidate.gasUrl.isEmpty() != candidate.gasToken.isEmpty()) ||
      (!candidate.gasUrl.isEmpty() && !candidate.wifiReady())) {
    message = F("Cloud requires Wi-Fi SSID, HTTPS URL and token");
    return false;
  }
  config = candidate;
  message = F("Configuration validated");
  return true;
}

inline LoadResult load(LoggerConfig& config, String& message) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    message = F("NVS open failed; using offline defaults");
    return LoadResult::Error;
  }
  if (!preferences.isKey(kKey)) {
    preferences.end();
    message = F("No NVS config; checking SD after USB setup window");
    return LoadResult::Missing;
  }
  const String json = preferences.getString(kKey);
  preferences.end();
  if (!parse(json.c_str(), config, message)) {
    message = F("NVS config invalid; using offline defaults");
    return LoadResult::Error;
  }
  message = config.cloudReady() ? F("NVS configuration loaded")
                                : F("NVS loaded; cloud disabled");
  return LoadResult::Loaded;
}

inline bool save(const LoggerConfig& config, String& message) {
  JsonDocument document;
  document["wifi_ssid"] = config.wifiSsid.c_str();
  document["wifi_password"] = config.wifiPassword.c_str();
  document["gas_url"] = config.gasUrl.c_str();
  document["gas_token"] = config.gasToken.c_str();
  document["ntp_server"] = config.ntpServer.c_str();
  document["timezone"] = config.timezone.c_str();
  document["sample_interval_sec"] = config.sampleIntervalSec;
  document["upload_batch_size"] = config.uploadBatchSize;
  document["deep_sleep_sec"] = config.deepSleepSec;
  char json[kMaxJsonBytes + 1];
  const size_t size = measureJson(document);
  if (document.overflowed() || size > kMaxJsonBytes) {
    message = F("Configuration serialization failed");
    return false;
  }
  serializeJson(document, json, sizeof(json));
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    message = F("NVS open failed");
    return false;
  }
  // A single NVS value avoids partially updated sets of credentials.
  const size_t written = preferences.putString(kKey, json);
  preferences.end();
  if (written != size) {
    message = F("NVS configuration write failed");
    return false;
  }
  message = F("Configuration saved");
  return true;
}

}  // namespace LoggerConfigNvs
