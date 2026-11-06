#include <SLT5006.h>
#include <assert.h>
#include <math.h>

#include <deque>
#include <iostream>
#include <vector>

#ifdef NDEBUG
#error "Host tests require assertions; build them without NDEBUG."
#endif

uint32_t fakeArduinoMillis = 0;

namespace {

struct Exchange {
  std::vector<uint8_t> request;
  std::vector<uint8_t> response;
};

uint16_t crc16(const std::vector<uint8_t>& data) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < data.size(); ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                      : static_cast<uint16_t>(crc >> 1);
    }
  }
  return crc;
}

std::vector<uint8_t> withCrc(std::vector<uint8_t> frame) {
  const uint16_t crc = crc16(frame);
  frame.push_back(static_cast<uint8_t>(crc >> 8));
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  return frame;
}

std::vector<uint8_t> readRequest(uint8_t address, uint8_t size) {
  return withCrc(std::vector<uint8_t>{0x01, address, size});
}

std::vector<uint8_t> response(uint8_t function, uint8_t address,
                              const std::vector<uint8_t>& data) {
  std::vector<uint8_t> frame{function, address,
                             static_cast<uint8_t>(data.size())};
  frame.insert(frame.end(), data.begin(), data.end());
  return withCrc(frame);
}

class FakeStream : public Stream {
 public:
  explicit FakeStream(const std::vector<Exchange>& exchanges)
      : exchanges_(exchanges), next_(0) {}

  int available() override { return incoming_.empty() ? 0 : 1; }

  int read() override {
    if (incoming_.empty()) return -1;
    const uint8_t value = incoming_.front();
    incoming_.pop_front();
    return value;
  }

  size_t write(const uint8_t* data, size_t size) override {
    assert(next_ < exchanges_.size());
    const std::vector<uint8_t> actual(data, data + size);
    assert(actual == exchanges_[next_].request);
    writes.push_back(actual);
    const std::vector<uint8_t>& reply = exchanges_[next_].response;
    incoming_.insert(incoming_.end(), reply.begin(), reply.end());
    ++next_;
    return size;
  }

  std::vector<std::vector<uint8_t> > writes;

 private:
  std::vector<Exchange> exchanges_;
  size_t next_;
  std::deque<uint8_t> incoming_;
};

class BrokenAvailableStream : public Stream {
 public:
  explicit BrokenAvailableStream(const std::vector<uint8_t>& expectedRequest)
      : expectedRequest_(expectedRequest), requestWritten_(false) {}

  int available() override { return requestWritten_ ? 1 : 0; }

  int read() override { return -1; }

  size_t write(const uint8_t* data, size_t size) override {
    assert(std::vector<uint8_t>(data, data + size) == expectedRequest_);
    requestWritten_ = true;
    return size;
  }

 private:
  std::vector<uint8_t> expectedRequest_;
  bool requestWritten_;
};

class ShortWriteStream : public Stream {
 public:
  int available() override { return 0; }
  int read() override { return -1; }
  size_t write(const uint8_t*, size_t size) override { return size - 1; }
};

void expectNear(float actual, float expected, float tolerance = 0.0001f) {
  assert(fabs(actual - expected) <= tolerance);
}

std::vector<uint8_t> measurementBlock(uint16_t temperature = 0x0010) {
  std::vector<uint8_t> block{
      0xC8,
      0x00,  // ADC battery = 200
      static_cast<uint8_t>(temperature & 0xFF),
      static_cast<uint8_t>(temperature >> 8),
      0xE8,
      0x03,  // bulk EC = 1.000
      0x0A,
      0x00,  // rock VWC = 1.0
      0x14,
      0x00,  // VWC = 2.0
      0x1E,
      0x00,  // coco VWC = 3.0
      0x00,
      0x00,  // unused
      0xD0,
      0x07,  // pore EC = 2.000
      0xB8,
      0x0B  // coco pore EC = 3.000
  };
  return block;
}

std::vector<Exchange> measurementExchanges(bool includeBusyState = true,
                                           uint16_t temperature = 0x0010) {
  std::vector<Exchange> result;
  result.push_back(
      Exchange{withCrc(std::vector<uint8_t>{0x02, 0x07, 0x01, 0x01}),
               response(0x02, 0x07, std::vector<uint8_t>{0x01})});
  if (includeBusyState) {
    result.push_back(
        Exchange{readRequest(0x08, 1),
                 response(0x01, 0x08, std::vector<uint8_t>{0x00})});
  }
  result.push_back(Exchange{readRequest(0x08, 1),
                            response(0x01, 0x08, std::vector<uint8_t>{0x01})});
  result.push_back(
      Exchange{readRequest(0x11, 18),
               response(0x01, 0x11, measurementBlock(temperature))});
  return result;
}

void testReadInfoAndKnownVectors() {
  fakeArduinoMillis = 0;
  FakeStream stream(std::vector<Exchange>{
      Exchange{std::vector<uint8_t>{0x01, 0x00, 0x03, 0x01, 0x60},
               response(0x01, 0x00, std::vector<uint8_t>{1, 7, 6})},
      Exchange{
          std::vector<uint8_t>{0x01, 0x03, 0x04, 0x33, 0x21},
          response(0x01, 0x03, std::vector<uint8_t>{0x39, 0x30, 0x00, 0x00})}});
  SLT5006 sensor(stream);
  SLT5006::SensorInfo info;
  assert(sensor.readInfo(info));
  assert(info.firmwareMajor == 1 && info.firmwareMinor == 7 &&
         info.firmwareRevision == 6);
  assert(info.serialNumber == 12345UL);
  assert(stream.writes.size() == 2);
}

void testMeasurementConversionAndTiming() {
  fakeArduinoMillis = 0;
  FakeStream stream(measurementExchanges());
  SLT5006 sensor(stream);
  SLT5006::Measurement value;
  assert(sensor.readMeasurement(value));
  assert(fakeArduinoMillis == 500UL);
  expectNear(value.batteryVoltageV, 200.0f * 3.3f / 4096.0f / 0.5f);
  expectNear(value.temperatureC, 1.0f);
  expectNear(value.ecBulkDsM, 1.0f);
  expectNear(value.vwcRockPercent, 1.0f);
  expectNear(value.vwcPercent, 2.0f);
  expectNear(value.vwcCocoPercent, 3.0f);
  expectNear(value.ecPoreDsM, 2.0f);
  expectNear(value.ecPoreCocoDsM, 3.0f);
}

void testNegativeTemperature() {
  fakeArduinoMillis = 0;
  FakeStream stream(measurementExchanges(false, 0x0FF0));
  SLT5006 sensor(stream);
  SLT5006::Measurement value;
  assert(sensor.readMeasurement(value));
  expectNear(value.temperatureC, -1.0f);
}

void testCrcAndDeviceErrors() {
  fakeArduinoMillis = 0;
  std::vector<uint8_t> bad =
      response(0x01, 0x00, std::vector<uint8_t>{1, 7, 6});
  bad.back() ^= 1;
  FakeStream badCrc(std::vector<Exchange>{Exchange{readRequest(0x00, 3), bad}});
  SLT5006 badCrcSensor(badCrc);
  SLT5006::SensorInfo info;
  assert(!badCrcSensor.readInfo(info));
  assert(badCrcSensor.lastError() == SLT5006::CRC_MISMATCH);

  FakeStream deviceError(std::vector<Exchange>{Exchange{
      readRequest(0x00, 3), withCrc(std::vector<uint8_t>{0x81, 0x02})}});
  SLT5006 deviceErrorSensor(deviceError);
  assert(!deviceErrorSensor.readInfo(info));
  assert(deviceErrorSensor.lastError() == SLT5006::DEVICE_ERROR);
  assert(deviceErrorSensor.lastDeviceErrorCode() == 0x02);
}

void testResponseHeaderAndWriteEchoValidation() {
  SLT5006::SensorInfo info;

  FakeStream wrongFunction(std::vector<Exchange>{
      Exchange{readRequest(0x00, 3),
               response(0x02, 0x00, std::vector<uint8_t>{1, 7, 6})}});
  SLT5006 wrongFunctionSensor(wrongFunction);
  assert(!wrongFunctionSensor.readInfo(info));
  assert(wrongFunctionSensor.lastError() == SLT5006::INVALID_RESPONSE);

  FakeStream wrongAddress(std::vector<Exchange>{
      Exchange{readRequest(0x00, 3),
               response(0x01, 0x01, std::vector<uint8_t>{1, 7, 6})}});
  SLT5006 wrongAddressSensor(wrongAddress);
  assert(!wrongAddressSensor.readInfo(info));
  assert(wrongAddressSensor.lastError() == SLT5006::INVALID_RESPONSE);

  FakeStream wrongSize(std::vector<Exchange>{Exchange{
      readRequest(0x00, 3), response(0x01, 0x00, std::vector<uint8_t>{1, 7})}});
  SLT5006 wrongSizeSensor(wrongSize);
  assert(!wrongSizeSensor.readInfo(info));
  assert(wrongSizeSensor.lastError() == SLT5006::INVALID_RESPONSE);

  FakeStream oversized(std::vector<Exchange>{
      Exchange{readRequest(0x00, 3), std::vector<uint8_t>{0x01, 0x00, 27}}});
  SLT5006 oversizedSensor(oversized);
  assert(!oversizedSensor.readInfo(info));
  assert(oversizedSensor.lastError() == SLT5006::INVALID_RESPONSE);

  FakeStream wrongAck(std::vector<Exchange>{
      Exchange{withCrc(std::vector<uint8_t>{0x02, 0x07, 0x01, 0x01}),
               response(0x02, 0x07, std::vector<uint8_t>{0x00})}});
  SLT5006 wrongAckSensor(wrongAck);
  assert(!wrongAckSensor.startMeasurement());
  assert(wrongAckSensor.lastError() == SLT5006::INVALID_RESPONSE);
}

void testPartialFrameTimeoutAndStateReset() {
  fakeArduinoMillis = 0;
  std::vector<uint8_t> partial =
      response(0x01, 0x00, std::vector<uint8_t>{1, 7, 6});
  partial.pop_back();
  FakeStream partialStream(
      std::vector<Exchange>{Exchange{readRequest(0x00, 3), partial}});
  SLT5006 partialSensor(partialStream);
  partialSensor.setResponseTimeout(5);
  SLT5006::SensorInfo info;
  assert(!partialSensor.readInfo(info));
  assert(partialSensor.lastError() == SLT5006::RESPONSE_TIMEOUT);

  fakeArduinoMillis = 0;
  FakeStream timeoutStream(std::vector<Exchange>{
      Exchange{withCrc(std::vector<uint8_t>{0x02, 0x07, 0x01, 0x01}),
               response(0x02, 0x07, std::vector<uint8_t>{0x01})}});
  SLT5006 timeoutSensor(timeoutStream);
  timeoutSensor.setMeasurementTimeout(300);
  SLT5006::Measurement value;
  assert(!timeoutSensor.readMeasurement(value));
  assert(timeoutSensor.lastError() == SLT5006::MEASUREMENT_TIMEOUT);
  bool complete = false;
  assert(!timeoutSensor.measurementComplete(complete));
  assert(timeoutSensor.lastError() == SLT5006::MEASUREMENT_NOT_READY);
}

void testInconsistentStreamAndShortWriteFailSafely() {
  fakeArduinoMillis = 0;
  BrokenAvailableStream broken(readRequest(0x00, 3));
  SLT5006 brokenSensor(broken);
  brokenSensor.setResponseTimeout(5);
  SLT5006::SensorInfo info;
  assert(!brokenSensor.readInfo(info));
  assert(brokenSensor.lastError() == SLT5006::RESPONSE_TIMEOUT);
  assert(fakeArduinoMillis == 5UL);

  ShortWriteStream shortWrite;
  SLT5006 shortWriteSensor(shortWrite);
  assert(!shortWriteSensor.readInfo(info));
  assert(shortWriteSensor.lastError() == SLT5006::WRITE_FAILED);
}

void testMalformedDeviceErrorsAreRejected() {
  SLT5006::SensorInfo info;

  std::vector<uint8_t> badCrc = withCrc(std::vector<uint8_t>{0x81, 0x02});
  badCrc.back() ^= 1;
  FakeStream badCrcStream(
      std::vector<Exchange>{Exchange{readRequest(0x00, 3), badCrc}});
  SLT5006 badCrcSensor(badCrcStream);
  assert(!badCrcSensor.readInfo(info));
  assert(badCrcSensor.lastError() == SLT5006::CRC_MISMATCH);
  assert(badCrcSensor.lastDeviceErrorCode() == 0);

  FakeStream wrongFunction(std::vector<Exchange>{Exchange{
      readRequest(0x00, 3), withCrc(std::vector<uint8_t>{0x82, 0x02})}});
  SLT5006 wrongFunctionSensor(wrongFunction);
  assert(!wrongFunctionSensor.readInfo(info));
  assert(wrongFunctionSensor.lastError() == SLT5006::INVALID_RESPONSE);
  assert(wrongFunctionSensor.lastDeviceErrorCode() == 0);
}

void testNonBlockingSettleAndPollInterval() {
  fakeArduinoMillis = 0;
  std::vector<Exchange> exchanges = measurementExchanges();
  FakeStream stream(exchanges);
  SLT5006 sensor(stream);
  assert(sensor.startMeasurement());
  bool complete = true;
  assert(sensor.measurementComplete(complete) && !complete);
  assert(stream.writes.size() == 1);
  delay(299);
  assert(sensor.measurementComplete(complete) && !complete);
  assert(stream.writes.size() == 1);
  delay(1);
  assert(sensor.measurementComplete(complete) && !complete);
  assert(stream.writes.size() == 2);
  assert(sensor.measurementComplete(complete) && !complete);
  assert(stream.writes.size() == 2);
  delay(200);
  assert(sensor.measurementComplete(complete) && complete);
  assert(stream.writes.size() == 3);
  SLT5006::Measurement value;
  assert(sensor.readMeasurementData(value));
}

void testNonBlockingTimeoutCanBeShorterThanSettleTime() {
  fakeArduinoMillis = 0;
  FakeStream stream(std::vector<Exchange>{
      Exchange{withCrc(std::vector<uint8_t>{0x02, 0x07, 0x01, 0x01}),
               response(0x02, 0x07, std::vector<uint8_t>{0x01})}});
  SLT5006 sensor(stream);
  sensor.setMeasurementTimeout(100);
  assert(sensor.startMeasurement());
  delay(100);
  bool complete = true;
  assert(!sensor.measurementComplete(complete));
  assert(!complete);
  assert(sensor.lastError() == SLT5006::MEASUREMENT_TIMEOUT);
  assert(stream.writes.size() == 1);
}

void testSingleMeasurementDataRequestTimeout() {
  fakeArduinoMillis = 0;
  std::vector<Exchange> exchanges = measurementExchanges(false);
  exchanges.back().response.clear();
  FakeStream stream(exchanges);
  SLT5006 sensor(stream);
  assert(sensor.startMeasurement());
  delay(300);
  bool complete = false;
  assert(sensor.measurementComplete(complete) && complete);
  const uint32_t beforeRead = fakeArduinoMillis;
  SLT5006::Measurement value;
  assert(!sensor.readMeasurementData(value));
  assert(sensor.lastError() == SLT5006::RESPONSE_TIMEOUT);
  assert(fakeArduinoMillis - beforeRead == 300UL);
  assert(stream.writes.size() == 3);
  assert(stream.writes.back() ==
         (std::vector<uint8_t>{0x01, 0x11, 0x12, 0x5D, 0xAC}));
  assert(!sensor.measurementComplete(complete));
  assert(sensor.lastError() == SLT5006::MEASUREMENT_NOT_READY);
}

void testTimeoutAcrossMillisWrap() {
  fakeArduinoMillis = 0xFFFFFFFDUL;
  FakeStream stream(std::vector<Exchange>{
      Exchange{readRequest(0x00, 3), std::vector<uint8_t>()}});
  SLT5006 sensor(stream);
  sensor.setResponseTimeout(5);
  SLT5006::SensorInfo info;
  assert(!sensor.readInfo(info));
  assert(sensor.lastError() == SLT5006::RESPONSE_TIMEOUT);
  assert(fakeArduinoMillis == 2UL);
}

void testFirmwareFeatureBoundary() {
  SLT5006::SensorInfo oldInfo = {1, 6, 3, 1};
  SLT5006::SensorInfo boundary = {1, 7, 5, 1};
  SLT5006::SensorInfo current = {1, 7, 6, 1};
  assert(!SLT5006::supportsEcPoreCoco(oldInfo));
  assert(SLT5006::supportsEcPoreCoco(boundary));
  assert(SLT5006::supportsEcPoreCoco(current));
}

}  // namespace

int main() {
  testReadInfoAndKnownVectors();
  testMeasurementConversionAndTiming();
  testNegativeTemperature();
  testCrcAndDeviceErrors();
  testResponseHeaderAndWriteEchoValidation();
  testPartialFrameTimeoutAndStateReset();
  testInconsistentStreamAndShortWriteFailSafely();
  testMalformedDeviceErrorsAreRejected();
  testNonBlockingSettleAndPollInterval();
  testNonBlockingTimeoutCanBeShorterThanSettleTime();
  testSingleMeasurementDataRequestTimeout();
  testTimeoutAcrossMillisWrap();
  testFirmwareFeatureBoundary();
  std::cout << "All SLT5006 host tests passed.\n";
  return 0;
}
