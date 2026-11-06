/*
  Safety: use one SLT5006 only (not an SLT5005 RS-232C connection).
  Upload this sketch before connecting the sensor. Wire only with all power off.
  Verify the sensor supply and UART logic levels, share GND, cross TX/RX, and
  use UART level conversion with powered-off protection when required.
  For this example connect WHITE to RED; insulate GREEN and ORANGE separately.
  Read ../../README.md (English) or ../../README.ja.md (Japanese) before power.
  Detailed wiring guides: ../../docs/wiring.en.md and ../../docs/wiring.md.

  This example cooperatively waits for conversion. Start/status requests can
  each block for the response timeout (300 ms by default); final data reading
  uses one request (about 300 ms with defaults). A completed status
  poll and final data reading are separated across loop() calls.
*/

#include <SLT5006.h>
#include <SoftwareSerial.h>

const uint8_t SENSOR_RX_PIN = 10;
const uint8_t SENSOR_TX_PIN = 11;
const uint32_t MEASUREMENT_INTERVAL_MS = 5000UL;

SoftwareSerial sensorSerial(SENSOR_RX_PIN, SENSOR_TX_PIN);
SLT5006 sensor(sensorSerial);

enum AppState { IDLE, MEASURING, DATA_READY };
AppState appState = IDLE;
uint32_t lastMeasurementAt = 0;
uint32_t lastBlinkAt = 0;
bool firstMeasurement = true;

void reportError() {
  Serial.print(F("SLT5006 error: "));
  Serial.println(sensor.errorMessage());
  if (sensor.lastError() == SLT5006::DEVICE_ERROR) {
    Serial.print(F("Sensor error code: 0x"));
    const uint8_t code = sensor.lastDeviceErrorCode();
    if (code < 0x10) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print(F(" ("));
    Serial.print(sensor.deviceErrorMessage());
    Serial.println(')');
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  sensorSerial.begin(SLT5006::BAUD_RATE);
  sensorSerial.listen();
  delay(1000);  // Conservative sample-side startup wait.
  Serial.println(F("SLT5006 cooperative polling example"));
}

void loop() {
  const uint32_t now = millis();

  // This represents unrelated application work that continues during sensing.
  if (static_cast<uint32_t>(now - lastBlinkAt) >= 500UL) {
    lastBlinkAt = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  if (appState == IDLE &&
      (firstMeasurement || static_cast<uint32_t>(now - lastMeasurementAt) >=
                               MEASUREMENT_INTERVAL_MS)) {
    firstMeasurement = false;
    if (sensor.startMeasurement()) {
      appState = MEASURING;
    } else {
      reportError();
      lastMeasurementAt = now;
    }
  }

  // Handle DATA_READY before MEASURING so a completed status poll and the final
  // data request never run back-to-back in one loop() call.
  if (appState == DATA_READY) {
    SLT5006::Measurement value;
    if (sensor.readMeasurementData(value)) {
      Serial.print(F("temperature_C="));
      Serial.print(value.temperatureC, 4);
      Serial.print(F(", vwc_percent="));
      Serial.print(value.vwcPercent, 1);
      Serial.print(F(", ec_bulk_dS_m="));
      Serial.println(value.ecBulkDsM, 3);
    } else {
      reportError();
    }
    appState = IDLE;
    lastMeasurementAt = millis();
  } else if (appState == MEASURING) {
    bool complete = false;
    if (!sensor.measurementComplete(complete)) {
      reportError();
      appState = IDLE;
      lastMeasurementAt = now;
    } else if (complete) {
      appState = DATA_READY;
    }
  }
}
