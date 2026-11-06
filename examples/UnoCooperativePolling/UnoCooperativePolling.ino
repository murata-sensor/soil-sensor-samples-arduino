/*
  Safety: use one SLT5006 only (not an SLT5005 RS-232C connection).
  Upload this sketch before connecting the sensor. Wire only with all power off.
  Verify the sensor supply and UART logic levels, share GND, cross TX/RX, and
  use UART level conversion with powered-off protection when required.
  For this example connect WHITE to RED; insulate GREEN and ORANGE separately.
  Read ../../README.md (English) or ../../README.ja.md (Japanese) before power.
  Detailed wiring guides: ../../docs/wiring.en.md and ../../docs/wiring.md.

  This example cooperatively waits for conversion while other loop work runs.
  The default 30-second measurement timeout is a limit, not a conversion time.
  Start/status requests can each block for the response timeout (300 ms by
  default); final data reading uses one request (about 300 ms with defaults).
  A completed status poll and final data reading are separated across loop()
  calls.
*/

#include <SLT5006.h>
#include <SoftwareSerial.h>

const uint8_t SENSOR_RX_PIN = 10;
const uint8_t SENSOR_TX_PIN = 11;
const uint32_t MEASUREMENT_INTERVAL_MS = 5000UL;

SoftwareSerial sensorSerial(SENSOR_RX_PIN, SENSOR_TX_PIN);
SLT5006 sensor(sensorSerial);
bool supportsEcPoreCoco = false;

enum AppState { IDLE, MEASURING, DATA_READY };
AppState appState = IDLE;
uint32_t lastMeasurementAt = 0;
uint32_t lastBlinkAt = 0;
bool firstMeasurement = true;

void printError() {
  if (sensor.lastError() == SLT5006::RESPONSE_TIMEOUT) {
    Serial.println(F("No response from SLT5006 (timeout)."));
    return;
  }
  Serial.print(F("Communication error: "));
  Serial.println(sensor.errorMessage());
  if (sensor.lastError() == SLT5006::DEVICE_ERROR) {
    const uint8_t code = sensor.lastDeviceErrorCode();
    Serial.print(F("Sensor error: 0x"));
    if (code < 0x10) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print(F(" ("));
    Serial.print(sensor.deviceErrorMessage());
    Serial.println(')');
  }
}

void printMeasurement(const SLT5006::Measurement& value) {
  Serial.println(F("--- SLT5006 measurement ---"));
  Serial.print(F("Temperature       : "));
  Serial.print(value.temperatureC, 4);
  Serial.println(F(" degC"));
  Serial.print(F("VWC               : "));
  Serial.print(value.vwcPercent, 1);
  Serial.println(F(" %"));
  Serial.print(F("VWC (rock wool)   : "));
  Serial.print(value.vwcRockPercent, 1);
  Serial.println(F(" %"));
  Serial.print(F("VWC (coco peat)   : "));
  Serial.print(value.vwcCocoPercent, 1);
  Serial.println(F(" %"));
  Serial.print(F("Bulk EC           : "));
  Serial.print(value.ecBulkDsM, 3);
  Serial.println(F(" dS/m"));
  Serial.print(F("Pore-water EC     : "));
  Serial.print(value.ecPoreDsM, 3);
  Serial.println(F(" dS/m"));
  Serial.print(F("Pore EC (coco)    : "));
  if (supportsEcPoreCoco) {
    Serial.print(value.ecPoreCocoDsM, 3);
    Serial.println(F(" dS/m"));
  } else {
    Serial.println(F("not shown: unsupported or firmware version unavailable"));
  }
  Serial.print(F("Supply voltage    : "));
  Serial.print(value.batteryVoltageV, 3);
  Serial.println(F(" V"));

  Serial.println();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  const uint32_t waitStartedAt = millis();
  while (!Serial && static_cast<uint32_t>(millis() - waitStartedAt) < 3000UL) {
  }

  sensorSerial.begin(SLT5006::BAUD_RATE);
  sensorSerial.listen();
  // A conservative sample-side wait, not a guaranteed sensor startup time.
  delay(1000);

  Serial.println(F("Murata SLT5006 cooperative polling"));
  SLT5006::SensorInfo info;
  bool infoAvailable = false;
  for (uint8_t attempt = 0; attempt < 3 && !infoAvailable; ++attempt) {
    infoAvailable = sensor.readInfo(info);
    if (!infoAvailable && attempt < 2) delay(500);
  }
  if (infoAvailable) {
    Serial.print(F("Firmware: "));
    Serial.print(info.firmwareMajor);
    Serial.print('.');
    Serial.print(info.firmwareMinor);
    Serial.print('.');
    Serial.println(info.firmwareRevision);
    Serial.print(F("Serial number: "));
    Serial.println(info.serialNumber);
    supportsEcPoreCoco = SLT5006::supportsEcPoreCoco(info);
  } else {
    printError();
    Serial.println(F("This is expected if the sensor is disconnected."));
    Serial.println(F("Otherwise, turn all power off and check sensor power,"));
    Serial.println(
        F("WHITE/Enable, common GND, UART levels, and crossed TX/RX."));
  }
  Serial.println();
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
      printError();
      Serial.println();
      lastMeasurementAt = now;
    }
  }

  // Handle DATA_READY before MEASURING so a completed status poll and the final
  // data request never run back-to-back in one loop() call.
  if (appState == DATA_READY) {
    SLT5006::Measurement measurement;
    if (sensor.readMeasurementData(measurement)) {
      printMeasurement(measurement);
    } else {
      printError();
      Serial.println();
    }
    appState = IDLE;
    lastMeasurementAt = millis();
  } else if (appState == MEASURING) {
    bool complete = false;
    if (!sensor.measurementComplete(complete)) {
      printError();
      Serial.println();
      appState = IDLE;
      lastMeasurementAt = now;
    } else if (complete) {
      appState = DATA_READY;
    }
  }
}
