#include <cassert>
#include <iostream>
#include <vector>

#include "../examples/M5Core2CloudLogger/LoggerConfigFile.h"
#include "../examples/M5Core2CloudLogger/LoggerSerialConfig.h"

namespace {
class FakeSerial : public Stream {
 public:
  std::string input;
  std::string output;
  size_t offset = 0;
  int available() override { return offset < input.size() ? 1 : 0; }
  int read() override {
    return offset < input.size() ? static_cast<unsigned char>(input[offset++])
                                 : -1;
  }
  size_t write(uint8_t byte) override {
    output += static_cast<char>(byte);
    return 1;
  }
};

bool receive(LoggerSerialConfig& receiver, FakeSerial& serial, uint32_t now,
             String& error) {
  bool saved = false;
  do {
    if (receiver.service(serial, now, error)) {
      saved = true;
      break;
    }
  } while (serial.available());
  return saved;
}

void testValidation() {
  String message;
  LoggerConfig config;
  assert(LoggerConfigNvs::parse("{}", config, message));
  assert(config.sampleIntervalSec == 60 && config.deepSleepSec == 0);
  assert(!config.wifiReady() && !config.cloudReady());
  const char* cloud =
      R"({"wifi_ssid":"wifi","wifi_password":" p ","gas_url":"https://example/exec","gas_token":"secret","sample_interval_sec":5,"upload_batch_size":20,"deep_sleep_sec":10})";
  assert(LoggerConfigNvs::parse(cloud, config, message));
  assert(config.cloudReady() && config.wifiPassword == " p ");
  assert(config.sampleIntervalSec == 5 && config.uploadBatchSize == 20);
  for (const char* json :
       {"",
        "[]",
        "{",
        "{} garbage",
        "{}{}",
        "{\"wifi_ssid\":null}",
        "{\"wifi_ssid\":\"a\\u0000b\"}",
        "{\"unknown_secret\":\"secret\"}",
        "{\"timezone\":\"\"}",
        "{\"ntp_server\":false}",
        "{\"sample_interval_sec\":4}",
        "{\"sample_interval_sec\":86401}",
        "{\"sample_interval_sec\":true}",
        "{\"sample_interval_sec\":60.0}",
        "{\"sample_interval_sec\":\"60\"}",
        "{\"upload_batch_size\":0}",
        "{\"upload_batch_size\":21}",
        "{\"deep_sleep_sec\":9}",
        "{\"deep_sleep_sec\":-1}",
        "{\"deep_sleep_sec\":86401}",
        "{\"gas_url\":\"http://example\"}",
        "{\"gas_url\":\"https://example\",\"gas_token\":\"t\"}",
        "{\"wifi_ssid\":\"w\",\"gas_token\":\"t\"}"}) {
    assert(!LoggerConfigNvs::parse(json, config, message));
    assert(
        config
            .cloudReady());  // Rejection leaves the previous RAM config intact.
    assert(message.find("unknown_secret") == std::string::npos);
  }
  for (const auto& limit :
       std::vector<std::pair<const char*, size_t>>{{"wifi_ssid", 32},
                                                   {"wifi_password", 64},
                                                   {"ntp_server", 253},
                                                   {"timezone", 128}}) {
    std::string json = "{\"" + std::string(limit.first) + "\":\"" +
                       std::string(limit.second, 'x') + "\"}";
    assert(LoggerConfigNvs::parse(json.c_str(), config, message));
    json.insert(json.size() - 2, "x");
    assert(!LoggerConfigNvs::parse(json.c_str(), config, message));
  }
  const std::string oversized(2049, ' ');
  assert(!LoggerConfigNvs::parse(oversized.c_str(), config, message));
  const std::string exact = "{}" + std::string(2046, ' ');
  assert(LoggerConfigNvs::parse(exact.c_str(), config, message));
  assert(LoggerConfigNvs::parse(
      "{\"sample_interval_sec\":86400,\"deep_sleep_sec\":86400}", config,
      message));
}

void testNvs() {
  Preferences::state() = Preferences::State();
  String message;
  LoggerConfig config;
  assert(LoggerConfigNvs::load(config, message) ==
         LoggerConfigNvs::LoadResult::Missing);
  assert(LoggerConfigNvs::parse(
      R"({"wifi_ssid":"wifi","wifi_password":"p","gas_url":"https://example","gas_token":"secret"})",
      config, message));
  assert(LoggerConfigNvs::save(config, message));
  const String original = Preferences::state().values[LoggerConfigNvs::kKey];
  LoggerConfig rebooted;
  assert(LoggerConfigNvs::load(rebooted, message) ==
         LoggerConfigNvs::LoadResult::Loaded);
  assert(rebooted.cloudReady() && rebooted.gasToken == "secret");
  Preferences::state().writeFails = true;
  assert(!LoggerConfigNvs::save(LoggerConfig(), message));
  assert(Preferences::state().values[LoggerConfigNvs::kKey] == original);
  Preferences::state().openFails = true;
  assert(!LoggerConfigNvs::save(config, message));
  assert(LoggerConfigNvs::load(rebooted, message) ==
         LoggerConfigNvs::LoadResult::Error);
  Preferences::state().openFails = false;
  Preferences::state().values[LoggerConfigNvs::kKey] = "corrupt";
  LoggerConfig defaults;
  assert(LoggerConfigNvs::load(defaults, message) ==
         LoggerConfigNvs::LoadResult::Error);
  assert(!defaults.wifiReady());
}

void testSerial() {
  Preferences::state() = Preferences::State();
  LoggerSerialConfig receiver;
  FakeSerial serial;
  String error;
  serial.input = "{}\n";
  assert(!receive(receiver, serial, 0, error));
  assert(Preferences::state().writes == 0);
  serial.input += "CONFIG BEGIN\r\n";
  assert(!receive(receiver, serial, 1, error));
  assert(receiver.active());
  serial.input += "{\"gas_token\":\"do-not-echo\"}\n";
  assert(!receive(receiver, serial, 2, error));
  assert(receiver.active() && Preferences::state().writes == 0);
  assert(serial.output.find("do-not-echo") == std::string::npos);
  serial.input += std::string(2049, 'x') + "\n";
  assert(!receive(receiver, serial, 3, error));
  serial.input += std::string("{\"wifi_ssid\":\"bad") + '\0' + "suffix\"}\n";
  assert(!receive(receiver, serial, 4, error));
  assert(Preferences::state().writes == 0);
  serial.input += "{";
  assert(!receive(receiver, serial, 5, error));
  assert(!receive(receiver, serial, 10005, error));
  serial.input +=
      "{}\n";  // A timed-out line's suffix must not become a new config.
  assert(!receive(receiver, serial, 10006, error));
  assert(Preferences::state().writes == 0);
  serial.input += "CONFIG CANCEL\n";
  assert(!receive(receiver, serial, 10007, error));
  assert(!receiver.active());
  serial.input += "CONFIG BEGIN\n{}\r\n";
  assert(receive(receiver, serial, 10008, error));
  assert(Preferences::state().writes == 1);
  assert(serial.output.find("[CONFIG] SAVED") != std::string::npos);
}

void testTimeoutAndWriteFailure() {
  Preferences::state() = Preferences::State();
  LoggerSerialConfig receiver;
  FakeSerial serial;
  String error;
  serial.input = "CONFIG BEGIN\n";
  const uint32_t start = UINT32_MAX - 1000;
  assert(!receive(receiver, serial, start, error));
  assert(!receive(receiver, serial, start + 119999, error));
  assert(receiver.active());
  assert(!receive(receiver, serial, start + 120000, error));
  assert(!receiver.active());
  serial.input += "CONFIG BEGIN\n{}\n";
  Preferences::state().writeFails = true;
  assert(!receive(receiver, serial, 120000, error));
  assert(serial.output.find("[CONFIG] SAVED") == std::string::npos);
  assert(receiver.active());
  Preferences::state().writeFails = false;
  serial.input += "{}\n";
  assert(receive(receiver, serial, 120001, error));
}

void testSerialSizeBoundary() {
  for (const char* newline : {"\n", "\r\n"}) {
    Preferences::state() = Preferences::State();
    LoggerSerialConfig receiver;
    FakeSerial serial;
    String error;
    serial.input = "CONFIG BEGIN\n{}" + std::string(2046, ' ') + newline;
    assert(receive(receiver, serial, 1, error));
    assert(Preferences::state().writes == 1);
  }

  Preferences::state() = Preferences::State();
  LoggerSerialConfig receiver;
  FakeSerial serial;
  String error;
  serial.input = "CONFIG BEGIN\n{}" + std::string(2047, ' ') + "\r\n";
  assert(!receive(receiver, serial, 1, error));
  assert(Preferences::state().writes == 0);
}

void testSdFallback() {
  using LoggerConfigNvs::LoadResult;
  Preferences::state() = Preferences::State();
  fs::FS sd;
  File file;
  file.valid = true;
  file.text =
      "# comment\r\n; comment\n\n wifi_ssid = \"SD Wi-Fi\"\r\n"
      "wifi_password=' p=word "
      "'\ngas_url=https://example/exec\ngas_token=token\n"
      "sample_interval_sec=30\nsample_interval_sec=45\nupload_batch_size=20\n"
      "deep_sleep_sec=10\nntp_server=pool.ntp.org\ntimezone=JST-9\n";
  sd.files[LoggerConfigFile::kPath] = file;
  LoggerConfig config;
  String message;
  assert(LoggerConfigFile::loadFallback(LoggerConfigNvs::load(config, message),
                                        &sd, config,
                                        message) == LoadResult::Loaded);
  assert(config.cloudReady() && config.wifiSsid == "SD Wi-Fi");
  assert(config.wifiPassword == " p=word " && config.sampleIntervalSec == 45);
  assert(Preferences::state().writes == 0);

  LoggerConfig nvsConfig = config;
  nvsConfig.wifiSsid = "NVS Wi-Fi";
  assert(LoggerConfigNvs::save(nvsConfig, message));
  LoggerConfig fromNvs;
  sd.checks = sd.opens = 0;
  assert(LoggerConfigFile::loadFallback(LoggerConfigNvs::load(fromNvs, message),
                                        &sd, fromNvs,
                                        message) == LoadResult::Loaded);
  assert(fromNvs.wifiSsid == "NVS Wi-Fi" && fromNvs.cloudReady());
  assert(sd.checks == 0 && sd.opens == 0);

  assert(LoggerConfigNvs::save(LoggerConfig(), message));
  LoggerConfig rebooted;
  sd.checks = sd.opens = 0;
  assert(LoggerConfigFile::loadFallback(
             LoggerConfigNvs::load(rebooted, message), &sd, rebooted,
             message) == LoadResult::Loaded);
  assert(!rebooted.wifiReady() && rebooted.sampleIntervalSec == 60);
  assert(sd.checks == 0 && sd.opens == 0);
  Preferences::state().values[LoggerConfigNvs::kKey] = "corrupt";
  assert(LoggerConfigFile::loadFallback(
             LoggerConfigNvs::load(rebooted, message), &sd, rebooted,
             message) == LoadResult::Error);
  assert(sd.checks == 0 && sd.opens == 0);
  Preferences::state().openFails = true;
  assert(LoggerConfigFile::loadFallback(
             LoggerConfigNvs::load(rebooted, message), &sd, rebooted,
             message) == LoadResult::Error);
  assert(sd.checks == 0 && sd.opens == 0);

  assert(LoggerConfigFile::load(nullptr, config, message) ==
         LoadResult::Missing);
  fs::FS empty;
  assert(LoggerConfigFile::load(&empty, config, message) ==
         LoadResult::Missing);
  for (const char* text : {"wifi_ssid=changed\ninvalid",
                           "sample_interval_sec=4", "sample_interval_sec=60.0",
                           "deep_sleep_sec=9", "gas_url=http://example",
                           "unknown=secret", "wifi_ssid=w\ngas_token=t"}) {
    assert(!LoggerConfigFile::parse(text, config, message));
    assert(config.wifiSsid == "SD Wi-Fi");
    assert(message.find("secret") == std::string::npos);
  }
  for (int failure = 0; failure < 5; ++failure) {
    File broken = file;
    if (failure == 0) broken.valid = false;
    if (failure == 1) broken.directory = true;
    if (failure == 2) broken.shortRead = true;
    if (failure == 3) broken.text = std::string(4097, ' ');
    if (failure == 4) broken.text = std::string("wifi_ssid=a") + '\0' + "b";
    sd.files[LoggerConfigFile::kPath] = broken;
    assert(LoggerConfigFile::load(&sd, config, message) == LoadResult::Error);
    assert(config.wifiSsid == "SD Wi-Fi");
  }
  file.text = "#" + std::string(4095, 'x');
  sd.files[LoggerConfigFile::kPath] = file;
  assert(LoggerConfigFile::load(&sd, config, message) == LoadResult::Loaded);
  assert(!config.wifiReady() && config.sampleIntervalSec == 60);
  assert(
      LoggerConfigFile::parse("\xef\xbb\xbfwifi_ssid=wifi", config, message));
  assert(config.wifiSsid == "wifi");
}
}  // namespace

int main() {
  testValidation();
  testNvs();
  testSerial();
  testTimeoutAndWriteFailure();
  testSerialSizeBoundary();
  testSdFallback();
  std::cout << "Logger config/NVS/USB tests passed\n";
}
