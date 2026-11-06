# Host unit tests

These tests compile the protocol driver against a minimal fake Arduino `Stream`. They verify known wire frames, the driver's single `0x11` measurement-read transaction, fragmented byte-by-byte reads, CRC and device errors, malformed responses, short writes, inconsistent custom `Stream` behavior, measurement conversion, the 300 ms settle time, 200 ms poll interval, firmware capability boundary, and timeout behavior across `millis()` wrap-around.

The portable CMake entry point is used by CI on Linux, macOS, and Windows:

```sh
cmake -S tests -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
```

Direct example with a C++11 compiler:

```powershell
g++ -std=c++11 -Wall -Wextra -Werror `
  -I tests\fake_arduino -I src `
  src\SLT5006.cpp tests\test_slt5006.cpp `
  -o $env:TEMP\slt5006_host_tests.exe
& $env:TEMP\slt5006_host_tests.exe

g++ -std=c++11 -Wall -Wextra -Werror `
  tests\test_logger_number_parser.cpp `
  -o $env:TEMP\logger_number_parser_tests.exe
& $env:TEMP\logger_number_parser_tests.exe

g++ -std=c++11 -Wall -Wextra -Werror `
  tests\test_upload_policy.cpp `
  -o $env:TEMP\upload_policy_tests.exe
& $env:TEMP\upload_policy_tests.exe
```

These host tests do not replace compiling the sketches for the target Arduino board or testing the actual sensor, UART logic-level conversion, wiring, and power supply.

`upload_policy_tests` checks redirect methods, acknowledgement requirements,
and closing the connection before HTTP cleanup using fakes. On Core2, also
verify a POST 302 followed by GET 200 and `ok:true` completes without a
cleanup-only TLS receive error, reports `accepted_rows`, and removes the batch.
Verify `Measuring...` is replaced by upload/result-fetch status before the
blocking requests, including retries via C. With offline Wi-Fi, a TLS failure,
or `ok:false`, confirm an explicit failure and that the queue is retained.
Host tests cannot verify actual TLS shutdown or display timing.

`queue_order_tests` checks bounded oldest-first selection with shuffled directory
order, empty timestamps first, filename tie-breaks, payload/path alignment,
batch size limits, a large backlog, and repeated successful batch draining.

## Logger configuration tests

After building `core2_cloud_logger` with PlatformIO, enable the JSON/NVS/USB host
tests using its installed ArduinoJson headers:

```powershell
cmake -S tests -B build\logger -DCMAKE_BUILD_TYPE=Debug `
  -DARDUINOJSON_INCLUDE_DIR="$PWD\.pio\libdeps\core2_cloud_logger\ArduinoJson\src"
cmake --build build\logger --config Debug --target logger_config_tests cloud_record_tests
ctest --test-dir build\logger -C Debug -R "^(logger_config_tests|cloud_record_tests)$" --output-on-failure
py -m unittest discover -s tests -p test_configure_logger.py -v
```

These tests cover validation limits/types, preserving config on rejection,
NVS save/reload and failures using a fake Preferences backend, serial framing,
NVS priority over SD (including offline settings), SD fallback only when NVS is
missing, INI parsing and SD read failures using a fake filesystem,
incomplete/oversized/NUL input, cancellation, timeout wrap-around, and explicit
save acknowledgements. Sender tests require only Python, not pyserial or hardware.
Real NVS persistence, USB connection/reset behavior, and SD/cloud operation still
require the acceptance checks in [logger setup](../docs/logger-setup.md).

`cloud_record_tests` checks the actual JSON/CSV serializers, all JSON control
character escapes, unavailable-value and firmware-dependent field shapes,
complete CSV header/row writes, queue retention after a short CSV write,
queue write/rename failures, ID collision protection, the exact 2048-byte
record limit, bounded oldest-first loading, malformed/trailing/NUL data,
short reads, and unreadable directories. Healthy queued rows can still be
uploaded while malformed files remain for investigation. These fakes do not
establish actual SD power-loss durability.

Google TLS root certificates can be checked offline with standard-library Python:

```powershell
py -m unittest discover -s tests -p test_google_root_ca.py -v
```

This verifies both PEM certificates parse as CAs, match the official R1/R4 DER
SHA-256 fingerprints, and are within their validity dates. It does not replace
TLS connection testing on the device's network.
