#ifndef MURATA_SLT5006_H
#define MURATA_SLT5006_H

#include <Arduino.h>

// Driver for one Murata SLT5006 soil sensor connected through a TTL UART.
// The caller owns and configures the Stream (SoftwareSerial, Serial1, etc.).
// A custom Stream must follow current Arduino flush() semantics: wait for
// pending transmit data (or do nothing when writes are synchronous), never
// discard received bytes.
class SLT5006 {
 public:
  static const uint32_t BAUD_RATE = 9600UL;

  struct SensorInfo {
    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    uint8_t firmwareRevision;
    uint32_t serialNumber;
  };

  struct Measurement {
    float batteryVoltageV;
    float temperatureC;
    float ecBulkDsM;
    float vwcRockPercent;
    float vwcPercent;
    float vwcCocoPercent;
    float ecPoreDsM;
    float ecPoreCocoDsM;
  };

  enum Error : uint8_t {
    NO_ERROR = 0,
    RESPONSE_TIMEOUT,
    WRITE_FAILED,
    INVALID_RESPONSE,
    CRC_MISMATCH,
    DEVICE_ERROR,
    MEASUREMENT_TIMEOUT,
    MEASUREMENT_NOT_READY
  };

  explicit SLT5006(Stream& sensorSerial);

  // Blocking convenience API. A normal measurement takes about one second.
  bool readInfo(SensorInfo& info);
  bool readMeasurement(Measurement& measurement);

  // Cooperative measurement API for integration into an application loop.
  // It avoids blocking for the whole conversion, but each UART transaction can
  // still wait up to the configured response timeout. Call startMeasurement(),
  // measurementComplete(), and then readMeasurementData() in that order.
  bool startMeasurement();
  bool measurementComplete(bool& complete);
  bool readMeasurementData(Measurement& measurement);

  void setResponseTimeout(uint32_t timeoutMs);
  void setMeasurementTimeout(uint32_t timeoutMs);
  void setPollInterval(uint32_t intervalMs);

  Error lastError() const;
  uint8_t lastDeviceErrorCode() const;
  const __FlashStringHelper* errorMessage() const;
  const __FlashStringHelper* deviceErrorMessage() const;

  // Firmware before 1.7.5 returns zero, but does not support this measurement.
  static bool supportsEcPoreCoco(const SensorInfo& info);

 private:
  static const uint8_t FUNCTION_READ = 0x01;
  static const uint8_t FUNCTION_WRITE = 0x02;
  static const uint8_t MAX_DATA_SIZE = 26;
  static const uint8_t MAX_RESPONSE_SIZE = 3 + MAX_DATA_SIZE + 2;

  static const uint8_t REG_VERSION = 0x00;
  static const uint8_t REG_SERIAL = 0x03;
  static const uint8_t REG_SENSOR_CONTROL = 0x07;
  static const uint8_t REG_SENSOR_STATE = 0x08;
  static const uint8_t REG_MEASUREMENT = 0x11;

  static const uint8_t SENSOR_CONTROL_START = 0x01;
  static const uint8_t SENSOR_STATE_DONE = 0x01;
  static const uint8_t MEASUREMENT_DATA_SIZE = 18;
  static const uint32_t START_SETTLE_MS = 300UL;
  static const uint32_t DEFAULT_RESPONSE_TIMEOUT_MS = 300UL;
  static const uint32_t DEFAULT_POLL_INTERVAL_MS = 200UL;
  static const uint32_t DEFAULT_MEASUREMENT_TIMEOUT_MS = 30000UL;

  Stream& serial_;
  uint32_t responseTimeoutMs_;
  uint32_t measurementTimeoutMs_;
  uint32_t pollIntervalMs_;
  uint32_t measurementStartedAtMs_;
  uint32_t lastPollAtMs_;
  bool measurementStarted_;
  bool measurementReady_;
  bool hasPolled_;
  Error lastError_;
  uint8_t lastDeviceErrorCode_;

  void clearError();
  void setError(Error error);
  void drainInput();
  bool readExact(uint8_t* destination, size_t size,
                 uint32_t responseStartedAtMs);
  bool transact(const uint8_t* request, size_t requestSize, uint8_t* response,
                size_t responseCapacity, size_t& responseSize);
  bool readRegisters(uint8_t address, uint8_t size, uint8_t* data);
  bool writeRegister(uint8_t address, uint8_t value);

  static uint16_t crc16(const uint8_t* data, size_t size);
  static bool validCrc(const uint8_t* frame, size_t size);
  static void appendCrcHighByteFirst(uint8_t* frame, size_t payloadSize);
  static uint16_t readLittleEndian16(const uint8_t* data);
  static int16_t signed12Bit(uint16_t raw);
};

#endif  // MURATA_SLT5006_H
