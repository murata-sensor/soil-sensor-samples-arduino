/*
  Safety: use one SLT5006 only (not an SLT5005 RS-232C connection).
  Upload this sketch before connecting the sensor. Wire only with all power off.
  Verify the sensor supply and UART logic levels, share GND, cross TX/RX, and
  use UART level conversion with powered-off protection when required.
  For this example connect WHITE to RED; insulate GREEN and ORANGE separately.
  Read ../../README.md (English) or ../../README.ja.md (Japanese) before power.
  Detailed wiring guides: ../../docs/wiring.en.md and ../../docs/wiring.md.
*/

#include <SLT5006.h>
#include <SoftwareSerial.h>

// Arduino Uno/Nano (classic) example.
// D10 receives from the sensor; D11 transmits to the sensor.
const uint8_t SENSOR_RX_PIN = 10;
const uint8_t SENSOR_TX_PIN = 11;
const uint32_t WAIT_BETWEEN_MEASUREMENTS_MS = 5000UL;

SoftwareSerial sensorSerial(SENSOR_RX_PIN, SENSOR_TX_PIN);
SLT5006 sensor(sensorSerial);
bool supportsEcPoreCoco = false;

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
  Serial.begin(115200);
  const uint32_t waitStartedAt = millis();
  while (!Serial && static_cast<uint32_t>(millis() - waitStartedAt) < 3000UL) {
  }

  sensorSerial.begin(SLT5006::BAUD_RATE);
  sensorSerial.listen();
  // A conservative sample-side wait, not a guaranteed sensor startup time.
  delay(1000);

  Serial.println(F("Murata SLT5006 quick start"));
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
  SLT5006::Measurement measurement;
  if (sensor.readMeasurement(measurement)) {
    printMeasurement(measurement);
  } else {
    printError();
    Serial.println();
  }
  delay(WAIT_BETWEEN_MEASUREMENTS_MS);
}
