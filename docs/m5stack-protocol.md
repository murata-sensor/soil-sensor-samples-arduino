# SLT5006 UART protocol

English | [日本語](m5stack-protocol.ja.md)

This reference covers only the public-register operations that the driver uses
to identify an SLT5006 and read measurements. For the complete register map,
electrical limits, and warranted behavior, see the latest product
documentation. Access only the registers that are published for customers.

The frames, timing, conversions, and API fields are compatible with the
SLT5006 implementations in `soil-sensor-samples-arduino` and
`soil-sensor-samples-python`.

## UART and frame format

Use 9600 bps, 8 data bits, no parity, one stop bit, and no flow control (8N1).
Connect only one sensor. The SLT5006 has no slave address, and the first byte
of each frame is the function code.

```text
Read request:        01  ADDRESS  SIZE  CRC_H  CRC_L
1-byte write:        02  ADDRESS  01  DATA  CRC_H  CRC_L
Normal response:     FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
Error response:      (REQUEST_FUNCTION | 0x80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE` is 1-26 bytes, so a normal response is at most 31 bytes. Frames have no
terminator, newline, STX, or ETX. If bit 7 of the first response byte is set,
the response is a four-byte error frame. Otherwise, `SIZE` in the three-byte
header determines how many bytes remain.

Known request frames for testing:

| Operation | Hex request |
|---|---|
| Read firmware | `01 00 03 01 60` |
| Read serial number | `01 03 04 33 21` |
| Start measurement | `02 07 01 01 0D 70` |
| Read state | `01 08 01 00 E6` |
| Read measurement values | `01 11 12 5D AC` |

## CRC

The CRC calculation is the same as CRC-16/MODBUS: initial value `0xFFFF` and
reflected polynomial `0xA001`. XOR each data byte into the CRC, then shift
LSB-first eight times. The SLT5006 sends the result **high byte first**. This
is the reverse of the usual MODBUS RTU wire order.

```text
crc = 0xFFFF
for byte in payload:
    crc ^= byte
    repeat 8 times:
        crc = (crc >> 1) ^ 0xA001  if (crc & 1) else (crc >> 1)
send (crc >> 8), (crc & 0xFF)
```

## Identification and measurement sequence

| Start address | Size | Value |
|---|---:|---|
| `0x00` | 3 | Firmware major, minor, revision (uint8 each) |
| `0x03` | 4 | Serial number (little-endian uint32) |

The Cloud Logger stores `firmware` as `major.minor.revision`. It stores
`serialNumber` as a decimal number in CSV and as a number in JSON. The included
GAS script normalizes it to a string for the spreadsheet.

Measurement flow:

1. Write `0x01` to `0x07`. Check the CRC, function code, address, size, and
   data of the echoed ACK.
2. Wait at least 300 ms after the ACK before the first state read.
3. Read one byte from `0x08` at intervals of at least 200 ms until bit 0 is set.
4. When 30 seconds (the default) have passed since the start ACK, do not start
   another state read. Report a measurement timeout. If a UART response wait is
   in progress at that point, the total time runs past 30 seconds.
5. Read 18 bytes from `0x11` and convert the public values.

The start ACK only means that the sensor accepted the command. Step 3 confirms
that the measurement is complete.

## Data conversion

Multi-byte values are little-endian (low byte first). The driver exposes these
values from the 18-byte block (nine words) at `0x11`:

| Word | C++ member | JSON / CSV | Conversion and unit |
|---:|---|---|---|
| 1 | `batteryVoltageV` | `battery_v` | `raw * 3.3 / 4096 / 0.5` V |
| 2 | `temperatureC` | `temperature_c` | signed low 12 bits x 0.0625 degC |
| 3 | `ecBulkDsM` | `ec_bulk_dsm` | uint16 x 0.001 dS/m |
| 4 | `vwcRockPercent` | `vwc_rock_pct` | uint16 x 0.1 % |
| 5 | `vwcPercent` | `vwc_pct` | uint16 x 0.1 % |
| 6 | `vwcCocoPercent` | `vwc_coco_pct` | uint16 x 0.1 % |
| 8 | `ecPoreDsM` | `ec_pore_dsm` | uint16 x 0.001 dS/m |
| 9 | `ecPoreCocoDsM` | `ec_pore_coco_dsm` | uint16 x 0.001 dS/m |

`batteryVoltageV` / `battery_v` is the converted value of the **sensor supply
voltage reported by the SLT5006**. The logger's `core2_battery_v` is a separate
value: the voltage of the Core2 internal battery.

For temperature, sign-extend the low 12 bits before multiplying. Do not read the
whole 16-bit word as a signed integer. The driver does not expose words that are
not listed in the table.

Coco-peat pore EC requires SLT5006 firmware 1.7.5 or later. Firmware 1.6.3 does
not support it, and firmware 1.7.5 and 1.7.6 do. Older firmware still lets you
read the whole block, including word 9, but word 9 is zero and is not a valid
measurement. If the firmware does not support this value or its version is
unknown, do not show the value on screen. Leave its column empty in the
fixed-column CSV, and omit the JSON property.

## Sensor error codes

| Code | Meaning |
|---:|---|
| `0x01` | Illegal function code |
| `0x02` | Illegal start address |
| `0x03` | Illegal byte size |
| `0x04` | Internal receive-buffer overflow |
| `0x05` | CRC-16 error |
| `0x06` | Data read while measuring |
| `0x10` | Internal storage write failure |
| `0x20` | Internal sensor communication error |
| `0x40` | Sensor-side measurement timeout |

Check the function code and CRC of an error response before you accept its
error code. Log unknown codes in hexadecimal instead of discarding them.

## Porting checklist

- Check that every request byte was written and that the full response arrives
  within the timeout, even when it arrives in pieces.
- Accept only `SIZE` values of 1-26, and keep each response within the fixed
  buffer.
- Validate the CRC, function code, address, size, and the complete write echo.
- Handle the four-byte error frame separately.
- Test the high-byte-first CRC, little-endian words, signed 12-bit temperature,
  and conversion factors.
- Enforce the 300 ms initial delay, the poll interval, and the measurement
  timeout.
- Use unsigned arithmetic for elapsed time so that `millis()` wraparound is
  handled correctly.
- After an error, do not resend the same request repeatedly. Clear stale input,
  wait, and retry from a new measurement cycle.

See the [integration guide](m5stack-integration.md) for application-level
retries, storage, and handling of firmware-dependent features.
