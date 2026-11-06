// SLT5006 quick-start for M5Stack Core2.

#include <M5Unified.h>
#include <SLT5006.h>

namespace {
enum class SensorPort { PORT_A, PORT_C };

// Use PORT_A for the Core2 built-in Grove port (G32/G33), or PORT_C for the
// blue UART port (G13/G14) on an M5GO Bottom2 or equivalent.
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;
constexpr int SENSOR_RX_PIN =
    SENSOR_PORT == SensorPort::PORT_A ? 32 : 13;  // Connect SLT5006 TX.
constexpr int SENSOR_TX_PIN =
    SENSOR_PORT == SensorPort::PORT_A ? 33 : 14;  // Connect SLT5006 RX.
constexpr const char* SENSOR_PORT_NAME =
    SENSOR_PORT == SensorPort::PORT_A ? "PORT.A" : "PORT.C";
constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;
constexpr uint32_t MEASUREMENT_TIMEOUT_MS = 32000;
constexpr uint32_t BATTERY_REFRESH_INTERVAL_MS = 5000;

HardwareSerial sensorUart(2);
SLT5006 sensor(sensorUart);
SLT5006::SensorInfo info{};
SLT5006::Measurement data{};
bool haveInfo = false;
bool haveMeasurement = false;
bool measurementStale = false;
bool measuring = false;
bool measurementRequested = false;
uint32_t cycleStartedAt = 0;
uint32_t nextPollAt = 0;
uint32_t nextSampleAt = 0;
uint32_t nextBatteryRefreshAt = 0;
bool batteryReadFailed = false;
String status = "Starting...";

bool due(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

String sensorError() {
  if (sensor.lastError() == SLT5006::RESPONSE_TIMEOUT) {
    return F("No response from SLT5006 (timeout).");
  }
  String text(sensor.errorMessage());
  if (sensor.lastDeviceErrorCode() != 0) {
    text += F(" / device: ");
    text += String(sensor.deviceErrorMessage());
  }
  return text;
}

void drawMeasurementRow(const char* label, float value, int precision,
                        const char* unit, int y) {
  char text[32];
  if (haveMeasurement) {
    snprintf(text, sizeof(text), "%.*f", precision, value);
  } else {
    snprintf(text, sizeof(text), "--");
  }
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(label, 8, y);
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(text, 192, y);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(unit, 204, y);
}

void drawCore2Battery() {
  const int32_t level = M5.Power.getBatteryLevel();
  const int16_t voltageMv = M5.Power.getBatteryVoltage();
  const bool valid = level >= 0 && level <= 100 && voltageMv > 0;
  char line[64];
  if (valid) {
    snprintf(line, sizeof(line), "Core2 battery (est.): %ld%% / %.2fV",
             static_cast<long>(level), voltageMv / 1000.0f);
  } else {
    snprintf(line, sizeof(line), "Core2 battery (est.): unavailable");
    if (!batteryReadFailed) {
      Serial.printf(
          "Core2 battery reading unavailable: level=%ld, voltage_mv=%d\n",
          static_cast<long>(level), static_cast<int>(voltageMv));
    }
  }
  batteryReadFailed = !valid;
  M5.Display.fillRect(0, 204, M5.Display.width(), 14, TFT_DARKGREY);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.drawString(line, 8, 207);
  nextBatteryRefreshAt = millis() + BATTERY_REFRESH_INTERVAL_MS;
}

void draw() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("SLT5006 Quick Start", 8, 6);
  M5.Display.drawFastHLine(0, 29, M5.Display.width(), TFT_DARKGREY);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  char portLine[40];
  snprintf(portLine, sizeof(portLine), "%s  RX:%d  TX:%d", SENSOR_PORT_NAME,
           SENSOR_RX_PIN, SENSOR_TX_PIN);
  M5.Display.drawString(portLine, 8, 35);
  if (haveInfo) {
    char line[80];
    snprintf(line, sizeof(line), "Serial: %lu   FW: %u.%u.%u",
             static_cast<unsigned long>(info.serialNumber),
             static_cast<unsigned>(info.firmwareMajor),
             static_cast<unsigned>(info.firmwareMinor),
             static_cast<unsigned>(info.firmwareRevision));
    M5.Display.drawString(line, 8, 51);
  }
  M5.Display.setTextColor(measuring ? TFT_YELLOW : TFT_CYAN, TFT_BLACK);
  M5.Display.drawString(status, 8, 68);

  if (measurementStale) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.drawString("Last good values (STALE)", 8, 81);
  }
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  drawMeasurementRow("Temp", data.temperatureC, 1, "C", 91);
  drawMeasurementRow("VWC", data.vwcPercent, 1, "%", 109);
  drawMeasurementRow("EC bulk", data.ecBulkDsM, 3, "dS/m", 127);
  drawMeasurementRow("EC pore", data.ecPoreDsM, 3, "dS/m", 145);
  drawMeasurementRow("Supply", data.batteryVoltageV, 2, "V", 163);
  M5.Display.setTextSize(1);
  char vwcLine[80];
  if (haveMeasurement) {
    snprintf(vwcLine, sizeof(vwcLine), "VWC rock: %.1f%%  VWC coco: %.1f%%",
             data.vwcRockPercent, data.vwcCocoPercent);
  } else {
    snprintf(vwcLine, sizeof(vwcLine), "VWC rock: --%%  VWC coco: --%%");
  }
  M5.Display.drawString(vwcLine, 8, 187);
  if (haveInfo && haveMeasurement && SLT5006::supportsEcPoreCoco(info)) {
    M5.Display.setCursor(8, 195);
    M5.Display.printf("EC pore coco: %.3f dS/m", data.ecPoreCocoDsM);
  }
  drawCore2Battery();
  M5.Display.fillRect(0, 218, M5.Display.width(), 22, TFT_NAVY);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.drawString("Button A: manual measurement", M5.Display.width() / 2,
                        229);
}

void fail(const __FlashStringHelper* operation) {
  Serial.print(operation);
  Serial.print(F(": "));
  Serial.println(sensorError());
  status = String(operation) + F(" failed: ") + sensorError();
  measuring = false;
  measurementStale = haveMeasurement;
  nextSampleAt = cycleStartedAt + SAMPLE_INTERVAL_MS;
  draw();
}

void startMeasurement() {
  cycleStartedAt = millis();
  if (!sensor.startMeasurement()) {
    fail(F("Start"));
    return;
  }
  measuring = true;
  nextPollAt = cycleStartedAt;
  status = F("Measuring...");
  draw();
}

void serviceMeasurement() {
  if (!measuring) return;
  const uint32_t now = millis();
  if (due(now, cycleStartedAt + MEASUREMENT_TIMEOUT_MS)) {
    status = F("Measurement timed out");
    measuring = false;
    measurementStale = haveMeasurement;
    nextSampleAt = cycleStartedAt + SAMPLE_INTERVAL_MS;
    Serial.println(status);
    draw();
    return;
  }
  if (!due(now, nextPollAt)) return;
  nextPollAt = now + 50;

  bool complete = false;
  if (!sensor.measurementComplete(complete)) {
    fail(F("Poll"));
    return;
  }
  if (!complete) return;
  if (!sensor.readMeasurementData(data)) {
    fail(F("Read"));
    return;
  }

  measuring = false;
  haveMeasurement = true;
  measurementStale = false;
  nextSampleAt = cycleStartedAt + SAMPLE_INTERVAL_MS;
  status = F("Measurement OK");
  Serial.println(F("--- SLT5006 measurement ---"));
  Serial.printf("temperature_c=%.2f\n", data.temperatureC);
  Serial.printf("vwc_percent=%.2f\n", data.vwcPercent);
  Serial.printf("vwc_rock_percent=%.2f\n", data.vwcRockPercent);
  Serial.printf("vwc_coco_percent=%.2f\n", data.vwcCocoPercent);
  Serial.printf("ec_bulk_ds_m=%.3f\n", data.ecBulkDsM);
  Serial.printf("ec_pore_ds_m=%.3f\n", data.ecPoreDsM);
  if (haveInfo && SLT5006::supportsEcPoreCoco(info)) {
    Serial.printf("ec_pore_coco_ds_m=%.3f\n", data.ecPoreCocoDsM);
  }
  Serial.printf("sensor_supply_v=%.2f\n", data.batteryVoltageV);
  draw();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  sensorUart.begin(SLT5006::BAUD_RATE, SERIAL_8N1, SENSOR_RX_PIN,
                   SENSOR_TX_PIN);
  draw();

  delay(1000);  // Conservative sensor start-up wait used by the Arduino sample.
  for (uint8_t attempt = 0; attempt < 3 && !haveInfo; ++attempt) {
    haveInfo = sensor.readInfo(info);
    if (!haveInfo && attempt < 2) delay(500);
  }
  if (haveInfo) {
    Serial.printf("SLT5006 serial=%lu firmware=%u.%u.%u\n",
                  static_cast<unsigned long>(info.serialNumber),
                  static_cast<unsigned>(info.firmwareMajor),
                  static_cast<unsigned>(info.firmwareMinor),
                  static_cast<unsigned>(info.firmwareRevision));
    status = F("Ready");
  } else {
    Serial.print(F("readInfo: "));
    Serial.println(sensorError());
    Serial.println(F("This is expected if the sensor is disconnected."));
    Serial.println(F("Otherwise, turn all power off and check sensor power,"));
    Serial.println(
        F("WHITE/Enable, common GND, UART levels, and crossed TX/RX."));
    status = F("Info unavailable; measurement will retry");
  }
  draw();
  nextSampleAt = millis();
}

void loop() {
  M5.update();
  serviceMeasurement();
  const uint32_t now = millis();
  if (due(now, nextBatteryRefreshAt)) drawCore2Battery();
  if (M5.BtnA.wasPressed()) measurementRequested = true;
  if (!measuring && (measurementRequested || due(now, nextSampleAt))) {
    measurementRequested = false;
    startMeasurement();
  }
  delay(2);  // Yield; sensor conversion is polled cooperatively above.
}
