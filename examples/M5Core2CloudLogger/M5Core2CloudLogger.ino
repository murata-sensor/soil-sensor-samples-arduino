// SLT5006 data logger for M5Stack Core2.
// Configuration is provisioned over USB Serial and stored in internal NVS.
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <SD.h>
#include <SLT5006.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <time.h>

#include "CloudRecord.h"
#include "GoogleRootCA.h"
#include "LoggerConfig.h"
#include "LoggerConfigFile.h"
#include "LoggerSerialConfig.h"
#include "UploadPolicy.h"

namespace {
enum class SensorPort { PORT_A, PORT_C };

// Use PORT_A for the Core2 built-in Grove port (G32/G33), or PORT_C for the
// blue UART port (G13/G14) on an M5GO Bottom2 or equivalent.
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;
constexpr int SENSOR_RX_PIN =
    SENSOR_PORT == SensorPort::PORT_A ? 32 : 13;  // Connect SLT5006 TX.
constexpr int SENSOR_TX_PIN =
    SENSOR_PORT == SensorPort::PORT_A ? 33 : 14;  // Connect SLT5006 RX.
constexpr uint32_t SENSOR_TIMEOUT_MS = 32000;
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t UPLOAD_RETRY_MS = 60000;
constexpr uint32_t SCREEN_REFRESH_MS = 500;
constexpr uint32_t BATTERY_REFRESH_MS = 5000;
constexpr uint32_t INITIAL_TIME_SYNC_TIMEOUT_MS = 10000;
constexpr size_t MAX_UPLOAD_BATCH = 20;

HardwareSerial sensorUart(2);
SLT5006 sensor(sensorUart);
SLT5006::SensorInfo sensorInfo{};
SLT5006::Measurement measurement{};
LoggerConfig config;
LoggerSerialConfig serialConfig;
M5Canvas screen(&M5.Display);
bool screenReady = false;

enum class SoilView : uint8_t { General, RockWool, Coco };
SoilView soilView = SoilView::General;
bool sdReady = false;
bool haveInfo = false;
bool haveMeasurement = false;
bool measurementStale = false;
bool measuring = false;
bool measurementRequested = false;
bool deepSleepPending = false;
bool deepSleepUploadAttempted = false;
bool initialTimeGateActive = false;
uint32_t measurementStartedAt = 0;
uint32_t currentCycleStartedAt = 0;
uint32_t nextPollAt = 0;
uint32_t nextSampleAt = 0;
uint32_t nextWifiAttemptAt = 0;
uint32_t nextUploadAt = 0;
uint32_t nextScreenAt = 0;
uint32_t initialTimeGateStartedAt = 0;
uint32_t sequenceNumber = 0;
String statusText = "Starting...";
String lastError;
uint32_t nextBatteryRefreshAt = 0;
int32_t batteryLevel = -1;
int16_t batteryVoltageMv = -1;
bool batteryReadingInitialized = false;
bool batteryReadFailed = false;

bool due(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

void setLastError(const String& message) {
  lastError = message;
  Serial.print(F("[ERROR] "));
  Serial.println(message);
}

void clearLastError() { lastError = ""; }

void scheduleNextSampleFromCycleStart() {
  nextSampleAt = currentCycleStartedAt + config.sampleIntervalSec * 1000UL;
}

bool clockIsValid() { return time(nullptr) >= 1704067200; }  // 2024-01-01

String sensorError() {
  String text(sensor.errorMessage());
  if (sensor.lastDeviceErrorCode()) {
    text += F(" / device ");
    text += sensor.lastDeviceErrorCode();
    text += F(": ");
    text += String(sensor.deviceErrorMessage());
  }
  return text;
}

String firmwareString() {
  if (!haveInfo) return String();
  char text[20];
  snprintf(text, sizeof(text), "%u.%u.%u", sensorInfo.firmwareMajor,
           sensorInfo.firmwareMinor, sensorInfo.firmwareRevision);
  return String(text);
}

String utcTimestamp() {
  const time_t now = time(nullptr);
  if (now < 1704067200) return String();
  struct tm utc;
  gmtime_r(&now, &utc);
  char text[25];
  strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(text);
}

String makeRecordId(const String& timestamp) {
  String compact = timestamp;
  compact.replace("-", "");
  compact.replace(":", "");
  compact.replace("T", "-");
  compact.replace("Z", "");
  if (!compact.length()) compact = String("boot-") + millis();
  const uint32_t serial = haveInfo ? sensorInfo.serialNumber : 0;
  char randomSuffix[9];
  snprintf(randomSuffix, sizeof(randomSuffix), "%08lx",
           static_cast<unsigned long>(esp_random()));
  return String(serial) + '-' + compact + '-' + sequenceNumber++ + '-' +
         randomSuffix;
}

void drawScreen();

void connectWifi() {
  nextWifiAttemptAt = millis() + WIFI_RETRY_MS;
  if (!config.wifiReady() || WiFi.status() == WL_CONNECTED) return;
  statusText = F("Connecting Wi-Fi...");
  drawScreen();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  const uint32_t deadline = millis() + WIFI_TIMEOUT_MS;
  while (WiFi.status() != WL_CONNECTED && !due(millis(), deadline)) {
    M5.update();
    delay(50);
  }
  if (WiFi.status() != WL_CONNECTED) {
    setLastError(F("Wi-Fi connection failed"));
    statusText = F("Offline; data queued on SD");
    return;
  }
  statusText = F("Wi-Fi connected");
  configTzTime(config.timezone.c_str(), config.ntpServer.c_str());
}

bool responseSaysOk(const String& response) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, response);
  return !error && document.is<JsonObject>() && document["ok"].is<bool>() &&
         document["ok"].as<bool>();
}

bool uploadQueuedRows() {
  if (!sdReady || !config.cloudReady()) return false;
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (WiFi.status() != WL_CONNECTED) return false;

  statusText = F("Preparing upload...");
  drawScreen();
  String paths[MAX_UPLOAD_BATCH];
  String rows;
  size_t count = 0;
  if (!CloudRecordFile::loadBatch(SD, config.uploadBatchSize, paths, rows,
                                  count)) {
    setLastError(F("SD queue read failed"));
    statusText = F("Upload failed; queue kept");
    return false;
  }
  if (!count) {
    statusText = F("No queued rows");
    return true;
  }

  String body;
  body.reserve(rows.length() + config.gasToken.length() + 40);
  body = F("{\"token\":\"");
  body += CloudRecordFile::jsonEscape(config.gasToken);
  body += F("\",\"rows\":[");
  body += rows;
  body += F("]}");

  int status = 0;
  String response;
  String url = config.gasUrl;
  bool post = true;
  for (uint8_t hop = 0; hop <= 5; ++hop) {
    // Use a fresh request/connection for the GAS result URL. Do not carry POST
    // headers, its body, or a previous connection into the result GET.
    statusText = post ? String(F("Uploading ")) + count + F(" row(s)...")
                      : String(F("Fetching upload result..."));
    drawScreen();
    WiFiClientSecure client;
    client.setCACert(GOOGLE_ROOT_CA);
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
      setLastError(F("HTTPS initialization failed"));
      statusText = F("Upload failed; queue kept");
      return false;
    }
    if (post) http.addHeader(F("Content-Type"), F("application/json"));
    status = post ? http.POST(body) : http.GET();
    Serial.printf("[UPLOAD] hop=%u method=%s status=%d\n",
                  static_cast<unsigned>(hop), post ? "POST" : "GET", status);
    if (status < 0) {
      char tlsError[160] = {};
      const int tlsCode = client.lastError(tlsError, sizeof(tlsError));
      Serial.printf(
          "[UPLOAD] transport=%d (%s), TLS=%d (%s), Wi-Fi=%d, RSSI=%d, "
          "clock_valid=%d\n",
          status, HTTPClient::errorToString(status).c_str(), tlsCode, tlsError,
          static_cast<int>(WiFi.status()), WiFi.RSSI(), clockIsValid());
    }
    const auto redirect = CloudUpload::redirectAction(status);
    if (redirect == CloudUpload::RedirectAction::Stop) {
      response = status > 0 ? http.getString() : String();
      CloudUpload::endRequest(client, http);
      break;
    }
    String location = http.getLocation();
    CloudUpload::endRequest(client, http);
    if (location.startsWith("/") && !location.startsWith("//")) {
      const int path = url.indexOf('/', 8);
      location = (path < 0 ? url : url.substring(0, path)) + location;
    }
    if (hop == 5 || !location.startsWith("https://")) {
      setLastError(F("Upload redirect limit or invalid HTTPS destination"));
      statusText = F("Upload failed; queue kept");
      return false;
    }
    url = location;
    if (redirect == CloudUpload::RedirectAction::Get) post = false;
  }
  if (!CloudUpload::responseAccepted(status, responseSaysOk(response))) {
    JsonDocument document;
    const bool jsonObject =
        !deserializeJson(document, response) && document.is<JsonObject>();
    Serial.printf("[UPLOAD] response_bytes=%u json_object=%d ok=%d\n",
                  static_cast<unsigned>(response.length()), jsonObject,
                  responseSaysOk(response));
    if (status < 0)
      setLastError(String(F("Upload transport ")) + status + F(": ") +
                   HTTPClient::errorToString(status));
    else
      setLastError(String(F("Upload failed: HTTP ")) + status);
    statusText = F("Upload failed; queue kept");
    return false;
  }
  bool queueCleaned = true;
  for (size_t i = 0; i < count; ++i) {
    if (SD.exists(paths[i]) && !SD.remove(paths[i])) queueCleaned = false;
  }
  if (!queueCleaned) {
    setLastError(F("Uploaded; queue cleanup failed"));
    statusText = F("Upload OK; retry cleanup later");
    return false;
  }
  clearLastError();
  statusText = String(F("Uploaded ")) + count + F(" row(s)");
  Serial.printf("[UPLOAD] accepted_rows=%u queue_cleaned=1\n",
                static_cast<unsigned>(count));
  return true;
}

CloudRecord makeErrorRecord(const String& error);
void persistRecord(const CloudRecord& row);
void finishSensorFailure(const String& error, const String& status);

void drawMeasurementRow(lgfx::LGFXBase& display, const char* label, float value,
                        int precision, const char* unit, int y) {
  char text[32];
  if (isfinite(value))
    snprintf(text, sizeof(text), "%.*f", precision, value);
  else
    snprintf(text, sizeof(text), "--");
  display.setTextDatum(top_left);
  display.drawString(label, 8, y);
  display.setTextDatum(top_right);
  display.drawString(text, 192, y);
  display.setTextDatum(top_left);
  display.drawString(unit, 204, y);
}

void drawCore2Battery(lgfx::LGFXBase& display) {
  const uint32_t now = millis();
  if (!batteryReadingInitialized || due(now, nextBatteryRefreshAt)) {
    batteryLevel = M5.Power.getBatteryLevel();
    batteryVoltageMv = M5.Power.getBatteryVoltage();
    const bool valid =
        batteryLevel >= 0 && batteryLevel <= 100 && batteryVoltageMv > 0;
    if (!valid && !batteryReadFailed) {
      Serial.printf(
          "[ERROR] Core2 battery reading unavailable: level=%ld, "
          "voltage_mv=%d\n",
          static_cast<long>(batteryLevel), static_cast<int>(batteryVoltageMv));
    }
    batteryReadFailed = !valid;
    batteryReadingInitialized = true;
    nextBatteryRefreshAt = now + BATTERY_REFRESH_MS;
  }
  char line[64];
  if (!batteryReadFailed) {
    snprintf(line, sizeof(line), "Core2 battery (est.): %ld%% / %.2fV",
             static_cast<long>(batteryLevel), batteryVoltageMv / 1000.0f);
  } else {
    snprintf(line, sizeof(line), "Core2 battery (est.): unavailable");
  }
  display.fillRect(0, 174, display.width(), 14, TFT_DARKGREY);
  display.setTextDatum(top_left);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.drawString(line, 8, 177);
}

void drawScreen() {
  lgfx::LGFXBase& display = screenReady
                                ? static_cast<lgfx::LGFXBase&>(screen)
                                : static_cast<lgfx::LGFXBase&>(M5.Display);
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_left);
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("SLT5006 Logger", 8, 5);
  display.setTextSize(1);
  display.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_ORANGE,
                       TFT_BLACK);
  display.drawString(
      WiFi.status() == WL_CONNECTED ? "Wi-Fi: online" : "Wi-Fi: offline", 220,
      10);
  display.setTextColor(sdReady ? TFT_GREEN : TFT_RED, TFT_BLACK);
  display.drawString(sdReady ? "SD: ready" : "SD: unavailable", 8, 31);
  display.setTextColor(measuring ? TFT_YELLOW : TFT_CYAN, TFT_BLACK);
  display.drawString(statusText, 82, 31);
  display.drawFastHLine(0, 48, display.width(), TFT_DARKGREY);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  if (!haveMeasurement) {
    display.drawString(measuring ? "Measuring..." : "No measurement yet", 35,
                       96);
  } else {
    const char* viewName = "General soil";
    float shownVwc = measurement.vwcPercent;
    float shownPoreEc = measurement.ecPoreDsM;
    if (soilView == SoilView::RockWool) {
      viewName = "Rock wool";
      shownVwc = measurement.vwcRockPercent;
    } else if (soilView == SoilView::Coco) {
      viewName = "Coconut peat";
      shownVwc = measurement.vwcCocoPercent;
      shownPoreEc = haveInfo && SLT5006::supportsEcPoreCoco(sensorInfo)
                        ? measurement.ecPoreCocoDsM
                        : NAN;
    }
    display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    display.drawString(
        measurementStale ? String(viewName) + F(" (STALE)") : String(viewName),
        8, 57);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    drawMeasurementRow(display, "Temp", measurement.temperatureC, 1, "C", 80);
    drawMeasurementRow(display, "VWC", shownVwc, 1, "%", 98);
    drawMeasurementRow(display, "EC bulk", measurement.ecBulkDsM, 3, "dS/m",
                       116);
    if (soilView != SoilView::RockWool)
      drawMeasurementRow(display, "EC pore", shownPoreEc, 3, "dS/m", 134);
    display.setTextSize(1);
    char supplyLine[40];
    snprintf(supplyLine, sizeof(supplyLine), "SLT5006 supply: %.2f V",
             measurement.batteryVoltageV);
    display.drawString(supplyLine, 8, 156);
  }
  drawCore2Battery(display);
  display.setTextDatum(top_left);
  display.setTextSize(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (haveInfo) {
    char sensorLine[64];
    snprintf(sensorLine, sizeof(sensorLine), "Serial: %lu  FW: %u.%u.%u",
             static_cast<unsigned long>(sensorInfo.serialNumber),
             static_cast<unsigned>(sensorInfo.firmwareMajor),
             static_cast<unsigned>(sensorInfo.firmwareMinor),
             static_cast<unsigned>(sensorInfo.firmwareRevision));
    display.drawString(sensorLine, 8, 190);
  } else {
    display.drawString("Serial: unavailable  FW: unavailable", 8, 190);
  }

  if (lastError.length()) {
    String visibleError = lastError;
    if (visibleError.length() > 45) {
      visibleError = visibleError.substring(0, 42) + F("...");
    }
    display.setTextDatum(top_left);
    display.setTextSize(1);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.drawString(String(F("ERR: ")) + visibleError, 8, 202);
  }

  display.fillRect(0, 216, display.width(), 24, TFT_NAVY);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.setTextDatum(middle_center);
  display.setTextSize(1);
  display.drawString("A Manual", 52, 222);
  display.drawString("measurement", 52, 232);
  display.drawString("B Change view", 160, 228);
  display.drawString("C Upload", 273, 228);
  if (screenReady) screen.pushSprite(0, 0);
}

void startMeasurement() {
  if (measuring) return;
  currentCycleStartedAt = millis();
  if (!sensor.startMeasurement()) {
    finishSensorFailure(sensorError(), F("Sensor start failed"));
    return;
  }
  measuring = true;
  measurementStartedAt = millis();
  nextPollAt = measurementStartedAt;
  statusText = F("Measuring...");
}

CloudRecord makeRecord() {
  CloudRecord row;
  row.timestamp = utcTimestamp();
  if (!row.timestamp.length()) row.error = F("time_not_synchronized");
  row.id = makeRecordId(row.timestamp);
  row.serialNumber = haveInfo ? sensorInfo.serialNumber : 0;
  row.firmware = firmwareString();
  row.sensorBatteryV = measurement.batteryVoltageV;
  row.setCore2BatteryMillivolts(M5.Power.getBatteryVoltage());
  row.temperatureC = measurement.temperatureC;
  row.vwcPercent = measurement.vwcPercent;
  row.vwcCocoPercent = measurement.vwcCocoPercent;
  row.vwcRockPercent = measurement.vwcRockPercent;
  row.ecBulkDsM = measurement.ecBulkDsM;
  row.ecPoreDsM = measurement.ecPoreDsM;
  row.cocoEcSupported = haveInfo && SLT5006::supportsEcPoreCoco(sensorInfo);
  if (row.cocoEcSupported) row.ecPoreCocoDsM = measurement.ecPoreCocoDsM;
  row.wifiRssiDbm = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;
  return row;
}

CloudRecord makeErrorRecord(const String& error) {
  CloudRecord row;
  row.timestamp = utcTimestamp();
  row.id = makeRecordId(row.timestamp);
  row.serialNumber = haveInfo ? sensorInfo.serialNumber : 0;
  row.firmware = firmwareString();
  row.setCore2BatteryMillivolts(M5.Power.getBatteryVoltage());
  row.wifiRssiDbm = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;
  row.error = error;
  if (!row.timestamp.length()) row.error += F(";time_not_synchronized");
  return row;
}

void persistRecord(const CloudRecord& row) {
  Serial.println(CloudRecordFile::toJson(row));
  if (sdReady) {
    // Queue first: cloud delivery is preserved even if CSV append later fails.
    const bool queued = CloudRecordFile::enqueue(SD, row);
    const bool logged = CloudRecordFile::appendCsv(SD, row);
    if (!queued || !logged) {
      setLastError(!queued ? F("SD queue write failed")
                           : F("SD CSV write failed"));
      statusText = F("Measurement stored incompletely");
    }
  }
  nextUploadAt = millis();
}

void finishSensorFailure(const String& error, const String& status) {
  measuring = false;
  measurementStale = haveMeasurement;
  setLastError(error);
  statusText = status;
  drawScreen();
  scheduleNextSampleFromCycleStart();
  persistRecord(makeErrorRecord(error));
  deepSleepPending = config.deepSleepSec >= 10;
  deepSleepUploadAttempted = false;
}

void finishMeasurement() {
  measuring = false;
  haveMeasurement = true;
  measurementStale = false;
  scheduleNextSampleFromCycleStart();
  clearLastError();
  statusText = F("Measurement OK");
  drawScreen();

  const CloudRecord row = makeRecord();
  if (row.error.length()) setLastError(row.error);
  persistRecord(row);
  deepSleepPending = config.deepSleepSec >= 10;
  deepSleepUploadAttempted = false;
}

void serviceMeasurement() {
  if (!measuring) return;
  const uint32_t now = millis();
  if (due(now, measurementStartedAt + SENSOR_TIMEOUT_MS)) {
    finishSensorFailure(F("sensor_timeout"), F("Sensor timed out"));
    return;
  }
  if (!due(now, nextPollAt)) return;
  nextPollAt = now + 50;
  bool complete = false;
  if (!sensor.measurementComplete(complete)) {
    finishSensorFailure(sensorError(), F("Sensor poll failed"));
    return;
  }
  if (!complete) return;
  if (!sensor.readMeasurementData(measurement)) {
    finishSensorFailure(sensorError(), F("Sensor read failed"));
    return;
  }
  finishMeasurement();
}

bool initializeSd() {
  // M5Unified initializes the Core2 shared SPI bus before this call.
  return SD.begin(GPIO_NUM_4, SPI, 25000000);
}

void serviceSerialConfig() {
  const bool wasActive = serialConfig.active();
  String error;
  if (serialConfig.service(Serial, millis(), error)) {
    statusText = F("NVS saved; restarting...");
    drawScreen();
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  if (error.length()) setLastError(error);
  if (serialConfig.active())
    statusText = F("USB configuration mode");
  else if (wasActive)
    statusText = F("USB configuration ended");
}

void readSensorInfoWithRetry() {
  for (uint8_t attempt = 0; attempt < 3 && !haveInfo; ++attempt) {
    haveInfo = sensor.readInfo(sensorInfo);
    if (!haveInfo) delay(500);
  }
  if (!haveInfo) setLastError(sensorError());
}

void beginInitialTimeGate() {
  if (WiFi.status() != WL_CONNECTED) return;
  initialTimeGateActive = true;
  initialTimeGateStartedAt = millis();
  statusText =
      clockIsValid() ? F("Time synchronized") : F("Synchronizing time...");
}

void serviceInitialTimeGate(uint32_t now) {
  if (!initialTimeGateActive) return;
  if (clockIsValid()) {
    initialTimeGateActive = false;
    nextSampleAt = now;
    statusText = F("Time synchronized");
    return;
  }
  if (due(now, initialTimeGateStartedAt + INITIAL_TIME_SYNC_TIMEOUT_MS)) {
    initialTimeGateActive = false;
    nextSampleAt = now;
    statusText = F("Time sync timeout; measuring offline");
    setLastError(F("NTP synchronization timed out"));
  }
}

void enterDeepSleep() {
  statusText = String(F("Sleeping ")) + config.deepSleepSec + F(" s");
  drawScreen();
  delay(100);
  Serial.flush();
  WiFi.disconnect(true);
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(config.deepSleepSec) *
                                1000000ULL);
  esp_deep_sleep_start();
}
}  // namespace

void setup() {
  const size_t rxBufferSize = Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  if (rxBufferSize < 4096)
    setLastError(F("USB receive buffer allocation failed"));
  auto m5Config = M5.config();
  M5.begin(m5Config);
  M5.Display.setRotation(1);
  screen.setColorDepth(16);
  screenReady =
      screen.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  if (!screenReady)
    setLastError(F("Screen buffer allocation failed; using direct drawing"));
  drawScreen();

  String configMessage;
  const auto loaded = LoggerConfigNvs::load(config, configMessage);
  statusText = configMessage;
  Serial.println(configMessage);
  if (loaded == LoggerConfigNvs::LoadResult::Error) setLastError(configMessage);
  Serial.println(F("[CONFIG] Send CONFIG BEGIN to configure NVS"));
  // Allow provisioning before sensor/Wi-Fi waits, including on deep-sleep
  // boots.
  const uint32_t bootAt = millis();
  while (millis() - bootAt < 3000 || serialConfig.active()) {
    M5.update();
    serviceSerialConfig();
    if (due(millis(), nextScreenAt)) {
      drawScreen();
      nextScreenAt = millis() + SCREEN_REFRESH_MS;
    }
    delay(2);
  }
  sensorUart.begin(SLT5006::BAUD_RATE, SERIAL_8N1, SENSOR_RX_PIN,
                   SENSOR_TX_PIN);
  sdReady = initializeSd();
  if (!sdReady) setLastError(F("No SD: logging and cloud upload disabled"));
  if (loaded == LoggerConfigNvs::LoadResult::Missing) {
    const auto fallback = LoggerConfigFile::loadFallback(
        loaded, sdReady ? &SD : nullptr, config, configMessage);
    statusText = configMessage;
    Serial.println(configMessage);
    if (fallback == LoggerConfigNvs::LoadResult::Error)
      setLastError(configMessage);
  }
  readSensorInfoWithRetry();
  connectWifi();
  nextSampleAt = millis();
  nextUploadAt = millis() + 5000;
  beginInitialTimeGate();
  drawScreen();
}

void loop() {
  M5.update();
  serviceSerialConfig();
  if (serialConfig.active()) {
    if (due(millis(), nextScreenAt)) {
      drawScreen();
      nextScreenAt = millis() + SCREEN_REFRESH_MS;
    }
    delay(2);
    return;
  }
  uint32_t now = millis();
  serviceInitialTimeGate(now);
  const bool measurementWasRunning = measuring;
  serviceMeasurement();
  now = millis();

  if (M5.BtnA.wasPressed()) measurementRequested = true;
  if (M5.BtnB.wasPressed()) {
    soilView = static_cast<SoilView>((static_cast<uint8_t>(soilView) + 1) % 3);
    nextScreenAt = now;
  }
  if (M5.BtnC.wasPressed()) {
    nextWifiAttemptAt = now;
    nextUploadAt = now;
  }

  // Give uploads an idle loop even when conversion exceeds the sample interval.
  if (!initialTimeGateActive && !deepSleepPending && !measuring &&
      !measurementWasRunning &&
      (measurementRequested || due(now, nextSampleAt))) {
    measurementRequested = false;
    startMeasurement();
  }
  if (!initialTimeGateActive && !measuring && config.wifiReady() &&
      WiFi.status() != WL_CONNECTED && due(now, nextWifiAttemptAt)) {
    connectWifi();
  }
  if (!initialTimeGateActive && !measuring && sdReady && config.cloudReady() &&
      due(now, nextUploadAt)) {
    uploadQueuedRows();
    drawScreen();
    if (deepSleepPending) deepSleepUploadAttempted = true;
    nextUploadAt = millis() + UPLOAD_RETRY_MS;
  }
  const bool uploadNeededBeforeSleep =
      sdReady && config.cloudReady() && !deepSleepUploadAttempted;
  if (deepSleepPending && !measuring && !uploadNeededBeforeSleep) {
    // Each wake measures immediately, attempts one upload, then sleeps. Thus
    // deep_sleep_sec is the effective interval; sample_interval_sec is ignored.
    enterDeepSleep();
  }
  if (due(now, nextScreenAt)) {
    drawScreen();
    nextScreenAt = now + SCREEN_REFRESH_MS;
  }
  delay(2);
}
