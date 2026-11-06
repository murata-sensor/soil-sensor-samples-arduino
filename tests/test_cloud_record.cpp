#include <Arduino.h>

#include <cassert>
#include <cstring>
#include <iostream>

#ifdef NDEBUG
#error "Host tests require assertions; build them without NDEBUG."
#endif

class FakeSerial : public Stream {
 public:
  std::string output;
  int available() override { return 0; }
  int read() override { return -1; }
  size_t write(uint8_t byte) override {
    output += static_cast<char>(byte);
    return 1;
  }
};

FakeSerial Serial;

#include "../examples/M5Core2CloudLogger/CloudRecord.h"

namespace {

CloudRecord makeRecord(const char* id = "record-1",
                       const char* timestamp = "2026-11-06T00:00:00Z") {
  CloudRecord record;
  record.id = id;
  record.timestamp = timestamp;
  record.serialNumber = 12345;
  record.firmware = "1.7.6";
  record.temperatureC = -1.25f;
  return record;
}

void addFile(fs::FS& fs, const char* path, const std::string& text,
             bool shortRead = false) {
  File file;
  file.valid = true;
  file.text = text;
  file.shortRead = shortRead;
  fs.files[path] = file;
}

void testRecordFormats() {
  CloudRecord record = makeRecord();
  const String json = CloudRecordFile::toJson(record);
  JsonDocument document;
  assert(LoggerJson::parseObject(json.c_str(), document));
  assert(document.as<JsonObjectConst>().size() == 14);
  assert(document["id"] == "record-1");
  assert(document["ts"] == record.timestamp.c_str());
  assert(document["serialNumber"].as<uint32_t>() == 12345);
  assert(document["firmware"] == "1.7.6");
  assert(document["temperature_c"].as<float>() == -1.25f);
  assert(document["rssi_dbm"].as<int32_t>() == -127);
  for (const char* key :
       {"battery_v", "core2_battery_v", "vwc_pct", "vwc_coco_pct",
        "vwc_rock_pct", "ec_bulk_dsm", "ec_pore_dsm"}) {
    assert(json.find(std::string("\"") + key + "\":null") != std::string::npos);
  }
  assert(json.find("ec_pore_coco_dsm") == std::string::npos);
  const std::string expected =
      "record-1,2026-11-06T00:00:00Z,12345,1.7.6,,,-1.25" +
      std::string(7, ',') + "-127,";
  assert(CloudRecordFile::toCsv(record) == expected);
  record.setCore2BatteryMillivolts(4000);
  assert(record.core2BatteryV == 4.0f);
  for (int16_t unavailable : {int16_t(-1), int16_t(0)}) {
    record.setCore2BatteryMillivolts(unavailable);
    assert(!isfinite(record.core2BatteryV));
    assert(CloudRecordFile::toCsv(record) == expected);
    assert(LoggerJson::parseObject(CloudRecordFile::toJson(record).c_str(),
                                   document));
    assert(document["core2_battery_v"].isNull());
  }
  record.cocoEcSupported = true;
  record.ecPoreCocoDsM = 1.087f;
  assert(LoggerJson::parseObject(CloudRecordFile::toJson(record).c_str(),
                                 document));
  assert(document.as<JsonObjectConst>().size() == 15);
  assert(document["ec_pore_coco_dsm"].as<float>() == 1.087f);
  assert(CloudRecordFile::toCsv(record).endsWith(",1.087,-127,"));
  record.error = "broken,\"sensor\"\n";
  assert(
      CloudRecordFile::toCsv(record).endsWith(",\"broken,\"\"sensor\"\"\n\""));
}

void testJsonEscaping() {
  CloudRecord record = makeRecord();
  for (uint8_t byte = 0; byte < 32; ++byte)
    record.error += static_cast<char>(byte);
  record.error += "\"\\\xc2\xa3";
  const String json = CloudRecordFile::toJson(record);
  JsonDocument document;
  assert(LoggerJson::parseObject(json.c_str(), document));
  const JsonString decoded = document["error"].as<JsonString>();
  assert(decoded.size() == record.error.length());
  assert(std::memcmp(decoded.c_str(), record.error.data(), decoded.size()) ==
         0);
}

void testCsvWrites() {
  const CloudRecord record = makeRecord();
  const std::string line = CloudRecordFile::toCsv(record) + "\r\n";
  fs::FS fs;
  assert(CloudRecordFile::appendCsv(fs, record));
  assert(fs.files[CloudRecordFile::kCsvPath].text ==
         std::string(CloudRecordFile::kCsvHeader) + "\r\n" + line);
  assert(CloudRecordFile::appendCsv(fs, record));
  assert(fs.files[CloudRecordFile::kCsvPath].text ==
         std::string(CloudRecordFile::kCsvHeader) + "\r\n" + line + line);
  assert(fs.files[CloudRecordFile::kCsvPath].flushes == 2);
  fs.openFails = true;
  assert(!CloudRecordFile::appendCsv(fs, record));

  fs::FS shortHeader;
  shortHeader.failWriteAt = 1;
  assert(!CloudRecordFile::appendCsv(shortHeader, record));
  assert(shortHeader.files[CloudRecordFile::kCsvPath].writes == 1);

  fs::FS shortRow;
  shortRow.failWriteAt = 2;
  assert(CloudRecordFile::enqueue(shortRow, record));
  assert(!CloudRecordFile::appendCsv(shortRow, record));
  assert(shortRow.files["/slt5006_queue/record-1.json"].text ==
         CloudRecordFile::toJson(record));
  assert(shortRow.files[CloudRecordFile::kCsvPath].writes == 2);
}

void testQueueWrites() {
  CloudRecord record = makeRecord();
  fs::FS fs;
  assert(CloudRecordFile::enqueue(fs, record));
  const std::string original = fs.files["/slt5006_queue/record-1.json"].text;
  const size_t opens = fs.opens;
  record.temperatureC = 99;
  assert(!CloudRecordFile::enqueue(fs, record));
  assert(fs.files["/slt5006_queue/record-1.json"].text == original);
  assert(fs.opens == opens);

  for (int failure = 0; failure < 5; ++failure) {
    fs::FS broken;
    if (failure == 0) broken.mkdirFails = true;
    if (failure == 1) broken.openFails = true;
    if (failure == 2) broken.writeLimit = 8;
    if (failure == 3) broken.renameFails = true;
    if (failure == 4) {
      assert(broken.mkdir(CloudRecordFile::kQueueDirectory));
      addFile(broken, "/slt5006_queue/record-1.json.tmp", "orphan");
      broken.removeFails = true;
    }
    assert(!CloudRecordFile::enqueue(broken, record));
    assert(!broken.exists("/slt5006_queue/record-1.json"));
    if (failure != 4)
      assert(!broken.exists("/slt5006_queue/record-1.json.tmp"));
    else
      assert(broken.files["/slt5006_queue/record-1.json.tmp"].text == "orphan");
  }

  record = makeRecord();
  const size_t base = CloudRecordFile::toJson(record).length();
  record.error = String(CloudRecordFile::kMaxRecordBytes - base, 'x');
  assert(CloudRecordFile::toJson(record).length() ==
         CloudRecordFile::kMaxRecordBytes);
  fs::FS boundary;
  assert(CloudRecordFile::enqueue(boundary, record));
  record.error += 'x';
  assert(CloudRecordFile::toJson(record).length() ==
         CloudRecordFile::kMaxRecordBytes + 1);
  fs::FS oversized;
  assert(!CloudRecordFile::enqueue(oversized, record));
  assert(oversized.opens == 0);
}

void testBatchLoading() {
  fs::FS fs;
  String paths[20];
  String rows = "old";
  size_t count = 20;
  assert(CloudRecordFile::loadBatch(fs, 2, paths, rows, count));
  assert(count == 0 && rows.empty());
  assert(fs.mkdir(CloudRecordFile::kQueueDirectory));
  addFile(fs, "/slt5006_queue/a-new.json",
          CloudRecordFile::toJson(makeRecord("new", "2026-11-06T01:00:00Z")));
  addFile(fs, "/slt5006_queue/z-old.json",
          CloudRecordFile::toJson(makeRecord("old")));
  addFile(fs, "/slt5006_queue/empty.json",
          CloudRecordFile::toJson(makeRecord("empty", "")));
  addFile(fs, "/slt5006_queue/corrupt.json",
          CloudRecordFile::toJson(makeRecord("corrupt")) + " garbage");
  addFile(fs, "/slt5006_queue/short.json",
          CloudRecordFile::toJson(makeRecord("short")) + "\n", true);
  addFile(fs, "/slt5006_queue/oversized.json",
          std::string(CloudRecordFile::kMaxRecordBytes + 1, 'x'));
  addFile(fs, "/slt5006_queue/nul.json",
          CloudRecordFile::toJson(makeRecord("nul")) + std::string(1, '\0'));
  addFile(fs, "/slt5006_queue/unfinished.json.tmp", "ignored");
  Serial.output.clear();
  assert(CloudRecordFile::loadBatch(fs, 2, paths, rows, count));
  assert(count == 2);
  assert(paths[0] == "/slt5006_queue/empty.json");
  assert(paths[1] == "/slt5006_queue/z-old.json");
  JsonDocument document;
  const String array = String("[") + rows + "]";
  assert(!deserializeJson(document, array.c_str()));
  assert(document.as<JsonArrayConst>().size() == 2);
  assert(document[0]["id"] == "empty" && document[1]["id"] == "old");
  assert(fs.files["/slt5006_queue/oversized.json"].reads == 0);
  assert(Serial.output.find("[ERROR] Ignoring malformed") != std::string::npos);
  assert(Serial.output.find("[ERROR] Ignoring oversized") != std::string::npos);
  assert(fs.remove(paths[0]) && fs.remove(paths[1]));
  assert(CloudRecordFile::loadBatch(fs, 2, paths, rows, count));
  assert(count == 1 && paths[0] == "/slt5006_queue/a-new.json");
  assert(fs.remove(paths[0]));
  assert(!CloudRecordFile::loadBatch(fs, 2, paths, rows, count));
  assert(count == 0 && rows.empty());
  for (uint8_t maximum : {uint8_t(0), uint8_t(21)})
    assert(!CloudRecordFile::loadBatch(fs, maximum, paths, rows, count));

  fs::FS unreadable;
  assert(unreadable.mkdir(CloudRecordFile::kQueueDirectory));
  unreadable.openFails = true;
  assert(!CloudRecordFile::loadBatch(unreadable, 2, paths, rows, count));
  unreadable.openFails = false;
  unreadable.files[CloudRecordFile::kQueueDirectory].directory = false;
  assert(!CloudRecordFile::loadBatch(unreadable, 2, paths, rows, count));
}

}  // namespace

int main() {
  testRecordFormats();
  testJsonEscaping();
  testCsvWrites();
  testQueueWrites();
  testBatchLoading();
  std::cout << "Cloud record/CSV/queue tests passed\n";
}
