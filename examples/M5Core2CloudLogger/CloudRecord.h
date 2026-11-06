#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <math.h>
#include <stdio.h>

#include "LoggerJson.h"
#include "QueueOrder.h"

struct CloudRecord {
  String id;
  String timestamp;
  uint32_t serialNumber = 0;
  String firmware;
  float sensorBatteryV = NAN;
  float core2BatteryV = NAN;
  float temperatureC = NAN;
  float vwcPercent = NAN;
  float vwcCocoPercent = NAN;
  float vwcRockPercent = NAN;
  float ecBulkDsM = NAN;
  float ecPoreDsM = NAN;
  float ecPoreCocoDsM = NAN;
  bool cocoEcSupported = false;
  int32_t wifiRssiDbm = -127;
  String error;

  void setCore2BatteryMillivolts(int16_t voltageMv) {
    core2BatteryV = voltageMv > 0 ? voltageMv / 1000.0f : NAN;
  }
};

namespace CloudRecordFile {

constexpr const char* kCsvPath = "/slt5006.csv";
constexpr const char* kQueueDirectory = "/slt5006_queue";
constexpr size_t kMaxRecordBytes = 2048;
constexpr const char* kCsvHeader =
    "id,ts,serialNumber,firmware,battery_v,core2_battery_v,temperature_c,"
    "vwc_pct,vwc_coco_pct,vwc_rock_pct,ec_bulk_dsm,ec_pore_dsm,"
    "ec_pore_coco_dsm,rssi_dbm,error";

inline String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '"':
        output += F("\\\"");
        break;
      case '\\':
        output += F("\\\\");
        break;
      case '\b':
        output += F("\\b");
        break;
      case '\f':
        output += F("\\f");
        break;
      case '\n':
        output += F("\\n");
        break;
      case '\r':
        output += F("\\r");
        break;
      case '\t':
        output += F("\\t");
        break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x",
                   static_cast<unsigned>(static_cast<uint8_t>(c)));
          output += escaped;
        } else {
          output += c;
        }
        break;
    }
  }
  return output;
}

inline String csvEscape(const String& input) {
  bool quote = false;
  String output;
  output.reserve(input.length() + 4);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == ',' || c == '"' || c == '\r' || c == '\n') quote = true;
    if (c == '"') output += '"';
    output += c;
  }
  return quote ? String('"') + output + '"' : output;
}

inline String jsonNumber(float value, uint8_t decimals) {
  return isfinite(value) ? String(value, static_cast<unsigned int>(decimals))
                         : F("null");
}

inline String csvNumber(float value, uint8_t decimals) {
  return isfinite(value) ? String(value, static_cast<unsigned int>(decimals))
                         : String();
}

inline String toJson(const CloudRecord& record) {
  String json;
  json.reserve(520);
  json += F("{\"id\":\"");
  json += jsonEscape(record.id);
  json += F("\",\"ts\":\"");
  json += jsonEscape(record.timestamp);
  json += F("\",\"serialNumber\":");
  json += record.serialNumber;
  json += F(",\"firmware\":\"");
  json += jsonEscape(record.firmware);
  json += F("\",\"battery_v\":");
  json += jsonNumber(record.sensorBatteryV, 3);
  json += F(",\"core2_battery_v\":");
  json += jsonNumber(record.core2BatteryV, 3);
  json += F(",\"temperature_c\":");
  json += jsonNumber(record.temperatureC, 2);
  json += F(",\"vwc_pct\":");
  json += jsonNumber(record.vwcPercent, 2);
  json += F(",\"vwc_coco_pct\":");
  json += jsonNumber(record.vwcCocoPercent, 2);
  json += F(",\"vwc_rock_pct\":");
  json += jsonNumber(record.vwcRockPercent, 2);
  json += F(",\"ec_bulk_dsm\":");
  json += jsonNumber(record.ecBulkDsM, 3);
  json += F(",\"ec_pore_dsm\":");
  json += jsonNumber(record.ecPoreDsM, 3);
  if (record.cocoEcSupported) {
    json += F(",\"ec_pore_coco_dsm\":");
    json += jsonNumber(record.ecPoreCocoDsM, 3);
  }
  json += F(",\"rssi_dbm\":");
  json += record.wifiRssiDbm;
  json += F(",\"error\":\"");
  json += jsonEscape(record.error);
  json += F("\"}");
  return json;
}

inline String toCsv(const CloudRecord& record) {
  String csv;
  csv.reserve(360);
  csv += csvEscape(record.id);
  csv += ',';
  csv += csvEscape(record.timestamp);
  csv += ',';
  csv += record.serialNumber;
  csv += ',';
  csv += csvEscape(record.firmware);
  csv += ',';
  csv += csvNumber(record.sensorBatteryV, 3);
  csv += ',';
  csv += csvNumber(record.core2BatteryV, 3);
  csv += ',';
  csv += csvNumber(record.temperatureC, 2);
  csv += ',';
  csv += csvNumber(record.vwcPercent, 2);
  csv += ',';
  csv += csvNumber(record.vwcCocoPercent, 2);
  csv += ',';
  csv += csvNumber(record.vwcRockPercent, 2);
  csv += ',';
  csv += csvNumber(record.ecBulkDsM, 3);
  csv += ',';
  csv += csvNumber(record.ecPoreDsM, 3);
  csv += ',';
  if (record.cocoEcSupported) csv += csvNumber(record.ecPoreCocoDsM, 3);
  csv += ',';
  csv += record.wifiRssiDbm;
  csv += ',';
  csv += csvEscape(record.error);
  return csv;
}

inline bool appendCsv(fs::FS& fs, const CloudRecord& record) {
  File file = fs.open(kCsvPath, FILE_APPEND);
  if (!file) return false;
  bool ok =
      file.size() != 0 || file.println(kCsvHeader) == strlen(kCsvHeader) + 2;
  if (ok) {
    const String line = toCsv(record);
    ok = file.println(line) == line.length() + 2;
  }
  file.flush();
  file.close();
  return ok;
}

inline bool enqueue(fs::FS& fs, const CloudRecord& record) {
  const String json = toJson(record);
  if (json.length() > kMaxRecordBytes) return false;
  if (!fs.exists(kQueueDirectory) && !fs.mkdir(kQueueDirectory)) return false;
  const String path = String(kQueueDirectory) + '/' + record.id + F(".json");
  if (fs.exists(path)) return false;
  const String temporary = path + F(".tmp");
  if (fs.exists(temporary) && !fs.remove(temporary)) return false;
  File file = fs.open(temporary, FILE_WRITE);
  if (!file) return false;
  const bool written = file.print(json) == json.length();
  file.flush();
  file.close();
  if (!written) {
    fs.remove(temporary);
    return false;
  }
  if (!fs.rename(temporary, path)) {
    fs.remove(temporary);
    return false;
  }
  return true;
}

inline String fullQueuePath(const String& name) {
  if (name.startsWith(kQueueDirectory)) return name;
  const int slash = name.lastIndexOf('/');
  const String leaf = slash >= 0 ? name.substring(slash + 1) : name;
  return String(kQueueDirectory) + '/' + leaf;
}

inline bool loadBatch(fs::FS& fs, uint8_t maximum, String paths[],
                      String& rowsJson, size_t& count) {
  rowsJson = "";
  count = 0;
  constexpr size_t kMaximumBatch = 20;
  if (maximum == 0 || maximum > kMaximumBatch) {
    Serial.println(F("[ERROR] Invalid queue batch size"));
    return false;
  }
  if (!fs.exists(kQueueDirectory)) return true;
  File directory = fs.open(kQueueDirectory);
  if (!directory || !directory.isDirectory()) {
    Serial.println(F("[ERROR] SD queue directory could not be opened"));
    return false;
  }

  struct QueueEntry {
    String timestamp;
    String path;
    String row;
  };
  QueueEntry selected[kMaximumBatch];
  bool malformedFiles = false;
  File entry;
  while ((entry = directory.openNextFile())) {
    const String name(entry.name());
    if (!entry.isDirectory() && name.endsWith(".json")) {
      if (entry.size() > kMaxRecordBytes) {
        malformedFiles = true;
        Serial.print(F("[ERROR] Ignoring oversized queue file: "));
        Serial.println(fullQueuePath(name));
        entry.close();
        continue;
      }
      String row = entry.readString();
      const bool completeRead = row.length() == entry.size();
      row.trim();
      JsonDocument document;
      if (completeRead && strlen(row.c_str()) == row.length() &&
          LoggerJson::parseObject(row.c_str(), document) &&
          document["id"].is<const char*>() &&
          document["ts"].is<const char*>()) {
        QueueEntry candidate;
        candidate.timestamp = document["ts"].as<const char*>();
        candidate.path = fullQueuePath(name);
        candidate.row = row;
        CloudQueue::selectOldest(selected, count, maximum, candidate);
      } else {
        malformedFiles = true;
        Serial.print(F("[ERROR] Ignoring malformed queue file: "));
        Serial.println(fullQueuePath(name));
      }
    }
    entry.close();
  }
  directory.close();
  for (size_t i = 0; i < count; ++i) {
    if (i) rowsJson += ',';
    rowsJson += selected[i].row;
    paths[i] = selected[i].path;
  }
  return count != 0 || !malformedFiles;
}

}  // namespace CloudRecordFile
