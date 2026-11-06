#include "SLT5006.h"

SLT5006::SLT5006(Stream& sensorSerial)
    : serial_(sensorSerial),
      responseTimeoutMs_(DEFAULT_RESPONSE_TIMEOUT_MS),
      measurementTimeoutMs_(DEFAULT_MEASUREMENT_TIMEOUT_MS),
      pollIntervalMs_(DEFAULT_POLL_INTERVAL_MS),
      measurementStartedAtMs_(0UL),
      lastPollAtMs_(0UL),
      measurementStarted_(false),
      measurementReady_(false),
      hasPolled_(false),
      lastError_(NO_ERROR),
      lastDeviceErrorCode_(0) {}

bool SLT5006::readInfo(SensorInfo& info) {
  clearError();
  uint8_t version[3];
  if (!readRegisters(REG_VERSION, sizeof(version), version)) return false;
  uint8_t serialNumber[4];
  if (!readRegisters(REG_SERIAL, sizeof(serialNumber), serialNumber))
    return false;
  info.firmwareMajor = version[0];
  info.firmwareMinor = version[1];
  info.firmwareRevision = version[2];
  info.serialNumber = static_cast<uint32_t>(serialNumber[0]) |
                      (static_cast<uint32_t>(serialNumber[1]) << 8) |
                      (static_cast<uint32_t>(serialNumber[2]) << 16) |
                      (static_cast<uint32_t>(serialNumber[3]) << 24);
  return true;
}

bool SLT5006::readMeasurement(Measurement& measurement) {
  if (!startMeasurement()) return false;
  // The sensor cannot accept a state read immediately after the start ACK.
  delay(START_SETTLE_MS);
  while (static_cast<uint32_t>(millis() - measurementStartedAtMs_) <
         measurementTimeoutMs_) {
    bool complete = false;
    if (!measurementComplete(complete)) return false;
    if (complete) return readMeasurementData(measurement);
    delay(pollIntervalMs_);
  }
  measurementStarted_ = false;
  measurementReady_ = false;
  hasPolled_ = false;
  setError(MEASUREMENT_TIMEOUT);
  return false;
}

bool SLT5006::startMeasurement() {
  clearError();
  measurementStarted_ = false;
  measurementReady_ = false;
  hasPolled_ = false;
  if (!writeRegister(REG_SENSOR_CONTROL, SENSOR_CONTROL_START)) return false;
  measurementStartedAtMs_ = millis();
  measurementStarted_ = true;
  return true;
}

bool SLT5006::measurementComplete(bool& complete) {
  clearError();
  complete = false;
  if (!measurementStarted_) {
    setError(MEASUREMENT_NOT_READY);
    return false;
  }
  if (measurementReady_) {
    complete = true;
    return true;
  }
  const uint32_t elapsed =
      static_cast<uint32_t>(millis() - measurementStartedAtMs_);
  if (elapsed >= measurementTimeoutMs_) {
    measurementStarted_ = false;
    measurementReady_ = false;
    hasPolled_ = false;
    setError(MEASUREMENT_TIMEOUT);
    return false;
  }
  if (elapsed < START_SETTLE_MS) return true;
  const uint32_t now = millis();
  if (hasPolled_ &&
      static_cast<uint32_t>(now - lastPollAtMs_) < pollIntervalMs_) {
    return true;
  }
  lastPollAtMs_ = now;
  hasPolled_ = true;
  uint8_t state = 0;
  if (!readRegisters(REG_SENSOR_STATE, 1, &state)) {
    measurementStarted_ = false;
    measurementReady_ = false;
    hasPolled_ = false;
    return false;
  }
  measurementReady_ = (state & SENSOR_STATE_DONE) != 0;
  complete = measurementReady_;
  return true;
}

bool SLT5006::readMeasurementData(Measurement& measurement) {
  clearError();
  if (!measurementReady_) {
    setError(MEASUREMENT_NOT_READY);
    return false;
  }
  uint8_t block[MEASUREMENT_DATA_SIZE];
  if (!readRegisters(REG_MEASUREMENT, sizeof(block), block)) {
    measurementStarted_ = false;
    measurementReady_ = false;
    hasPolled_ = false;
    return false;
  }

  const uint16_t adcBattery = readLittleEndian16(block);
  measurement.batteryVoltageV =
      static_cast<float>(adcBattery) * 3.3f / 4096.0f / 0.5f;
  measurement.temperatureC =
      static_cast<float>(signed12Bit(readLittleEndian16(block + 2))) * 0.0625f;
  measurement.ecBulkDsM =
      static_cast<float>(readLittleEndian16(block + 4)) * 0.001f;
  measurement.vwcRockPercent =
      static_cast<float>(readLittleEndian16(block + 6)) * 0.1f;
  measurement.vwcPercent =
      static_cast<float>(readLittleEndian16(block + 8)) * 0.1f;
  measurement.vwcCocoPercent =
      static_cast<float>(readLittleEndian16(block + 10)) * 0.1f;
  // block + 12 is not exposed by the customer-facing API.
  measurement.ecPoreDsM =
      static_cast<float>(readLittleEndian16(block + 14)) * 0.001f;
  measurement.ecPoreCocoDsM =
      static_cast<float>(readLittleEndian16(block + 16)) * 0.001f;
  measurementStarted_ = false;
  measurementReady_ = false;
  hasPolled_ = false;
  return true;
}

void SLT5006::setResponseTimeout(uint32_t timeoutMs) {
  responseTimeoutMs_ = timeoutMs == 0 ? 1 : timeoutMs;
}

void SLT5006::setMeasurementTimeout(uint32_t timeoutMs) {
  measurementTimeoutMs_ = timeoutMs == 0 ? 1 : timeoutMs;
}

void SLT5006::setPollInterval(uint32_t intervalMs) {
  pollIntervalMs_ = intervalMs == 0 ? 1 : intervalMs;
}

SLT5006::Error SLT5006::lastError() const { return lastError_; }

uint8_t SLT5006::lastDeviceErrorCode() const { return lastDeviceErrorCode_; }

const __FlashStringHelper* SLT5006::errorMessage() const {
  switch (lastError_) {
    case NO_ERROR:
      return F("no error");
    case RESPONSE_TIMEOUT:
      return F("sensor response timed out");
    case WRITE_FAILED:
      return F("UART request write was incomplete");
    case INVALID_RESPONSE:
      return F("sensor response format was invalid");
    case CRC_MISMATCH:
      return F("sensor response CRC did not match");
    case DEVICE_ERROR:
      return F("sensor returned a device error");
    case MEASUREMENT_TIMEOUT:
      return F("measurement did not finish in time");
    case MEASUREMENT_NOT_READY:
      return F("measurement has not completed");
    default:
      return F("unknown error");
  }
}

const __FlashStringHelper* SLT5006::deviceErrorMessage() const {
  switch (lastDeviceErrorCode_) {
    case 0x01:
      return F("illegal function code");
    case 0x02:
      return F("illegal start address");
    case 0x03:
      return F("illegal byte size");
    case 0x04:
      return F("internal receive-buffer overflow");
    case 0x05:
      return F("CRC-16 error");
    case 0x06:
      return F("data read while measurement is in progress");
    case 0x10:
      return F("write failure to internal storage");
    case 0x20:
      return F("internal sensor communication error");
    case 0x40:
      return F("measurement timeout");
    default:
      return F("unknown sensor error");
  }
}

bool SLT5006::supportsEcPoreCoco(const SensorInfo& info) {
  if (info.firmwareMajor != 1) return info.firmwareMajor > 1;
  if (info.firmwareMinor != 7) return info.firmwareMinor > 7;
  return info.firmwareRevision >= 5;
}

void SLT5006::clearError() {
  lastError_ = NO_ERROR;
  lastDeviceErrorCode_ = 0;
}

void SLT5006::setError(Error error) { lastError_ = error; }

void SLT5006::drainInput() {
  while (serial_.available() > 0) {
    // Some Stream implementations can report a byte and still return -1 when
    // an interrupt consumes it first. Do not spin forever on that race.
    if (serial_.read() < 0) break;
  }
}

bool SLT5006::readExact(uint8_t* destination, size_t size,
                        uint32_t responseStartedAtMs) {
  size_t received = 0;
  while (received < size) {
    while (serial_.available() > 0 && received < size) {
      const int value = serial_.read();
      // Re-check the timeout if a Stream temporarily has no readable byte even
      // though available() was positive. This also protects custom Streams
      // with inconsistent available()/read() implementations.
      if (value < 0) break;
      destination[received++] = static_cast<uint8_t>(value);
    }
    if (received == size) return true;
    if (static_cast<uint32_t>(millis() - responseStartedAtMs) >=
        responseTimeoutMs_) {
      setError(RESPONSE_TIMEOUT);
      return false;
    }
    delay(1);
  }
  return true;
}

bool SLT5006::transact(const uint8_t* request, size_t requestSize,
                       uint8_t* response, size_t responseCapacity,
                       size_t& responseSize) {
  drainInput();
  if (responseCapacity < 3) {
    setError(INVALID_RESPONSE);
    return false;
  }
  if (serial_.write(request, requestSize) != requestSize) {
    setError(WRITE_FAILED);
    return false;
  }
  // Start the response timeout after the final UART bit has left buffered
  // HardwareSerial implementations. SoftwareSerial has no TX buffer.
  serial_.flush();
  const uint32_t responseStartedAtMs = millis();
  if (!readExact(response, 1, responseStartedAtMs)) return false;

  if ((response[0] & 0x80) != 0) {
    responseSize = 4;
    if (responseCapacity < responseSize) {
      setError(INVALID_RESPONSE);
      return false;
    }
    if (!readExact(response + 1, responseSize - 1, responseStartedAtMs)) {
      return false;
    }
    if (!validCrc(response, responseSize)) {
      setError(CRC_MISMATCH);
      return false;
    }
    if (response[0] != static_cast<uint8_t>(request[0] | 0x80)) {
      setError(INVALID_RESPONSE);
      return false;
    }
    lastDeviceErrorCode_ = response[1];
    setError(DEVICE_ERROR);
    return false;
  }

  if (!readExact(response + 1, 2, responseStartedAtMs)) return false;
  const uint8_t dataSize = response[2];
  if (dataSize > MAX_DATA_SIZE) {
    setError(INVALID_RESPONSE);
    return false;
  }
  responseSize = static_cast<size_t>(3 + dataSize + 2);
  if (responseSize > responseCapacity) {
    setError(INVALID_RESPONSE);
    return false;
  }
  if (!readExact(response + 3, static_cast<size_t>(dataSize + 2),
                 responseStartedAtMs)) {
    return false;
  }
  return true;
}

bool SLT5006::readRegisters(uint8_t address, uint8_t size, uint8_t* data) {
  if (size == 0 || size > MAX_DATA_SIZE ||
      static_cast<uint16_t>(address) + size - 1 > 0xFF) {
    setError(INVALID_RESPONSE);
    return false;
  }
  uint8_t request[5] = {FUNCTION_READ, address, size, 0, 0};
  appendCrcHighByteFirst(request, 3);
  uint8_t response[MAX_RESPONSE_SIZE];
  size_t responseSize = 0;
  if (!transact(request, sizeof(request), response, sizeof(response),
                responseSize)) {
    return false;
  }
  if (!validCrc(response, responseSize)) {
    setError(CRC_MISMATCH);
    return false;
  }
  if (response[0] != FUNCTION_READ || response[1] != address ||
      response[2] != size) {
    setError(INVALID_RESPONSE);
    return false;
  }
  for (uint8_t i = 0; i < size; ++i) data[i] = response[3 + i];
  return true;
}

bool SLT5006::writeRegister(uint8_t address, uint8_t value) {
  uint8_t request[6] = {FUNCTION_WRITE, address, 1, value, 0, 0};
  appendCrcHighByteFirst(request, 4);
  uint8_t response[MAX_RESPONSE_SIZE];
  size_t responseSize = 0;
  if (!transact(request, sizeof(request), response, sizeof(response),
                responseSize)) {
    return false;
  }
  if (!validCrc(response, responseSize)) {
    setError(CRC_MISMATCH);
    return false;
  }
  if (responseSize != 6 || response[0] != FUNCTION_WRITE ||
      response[1] != address || response[2] != 1 || response[3] != value) {
    setError(INVALID_RESPONSE);
    return false;
  }
  return true;
}

uint16_t SLT5006::crc16(const uint8_t* data, size_t size) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if ((crc & 0x0001) != 0) {
        crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool SLT5006::validCrc(const uint8_t* frame, size_t size) {
  if (size < 3) return false;
  const uint16_t expected = crc16(frame, size - 2);
  const uint16_t received =
      (static_cast<uint16_t>(frame[size - 2]) << 8) | frame[size - 1];
  return received == expected;
}

void SLT5006::appendCrcHighByteFirst(uint8_t* frame, size_t payloadSize) {
  const uint16_t crc = crc16(frame, payloadSize);
  frame[payloadSize] = static_cast<uint8_t>(crc >> 8);
  frame[payloadSize + 1] = static_cast<uint8_t>(crc & 0xFF);
}

uint16_t SLT5006::readLittleEndian16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

int16_t SLT5006::signed12Bit(uint16_t raw) {
  int16_t value = static_cast<int16_t>(raw & 0x0FFF);
  if ((value & 0x0800) != 0) {
    value = static_cast<int16_t>(value - 0x1000);
  }
  return value;
}
