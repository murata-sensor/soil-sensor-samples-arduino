# SLT5006 UART protocol

English | [日本語](m5stack-protocol.ja.md)

This reference documents the minimum public-register flow used to identify and
measure an SLT5006. Consult the current product documentation for the complete
register map, electrical limits, and warranted behavior. Do not access
non-customer registers.

The frames, timing, conversions, and API fields are compatible with the
SLT5006 implementations in `soil-sensor-samples-arduino` and
`soil-sensor-samples-python`.

## UART and frame format

Use 9600 bps, 8 data bits, no parity, one stop bit, and no flow control (8N1).
Only one sensor may be connected; there is no slave address.

```text
Read request:        01  ADDRESS  SIZE  CRC_H  CRC_L
1-byte write:        02  ADDRESS  01  DATA  CRC_H  CRC_L
Normal response:     FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
Error response:      (REQUEST_FUNCTION | 0x80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE` is 1-26 bytes, so a normal response is at most 31 bytes. There is no
terminator, newline, STX, or ETX. Bit 7 of the first response byte identifies
the four-byte error form; otherwise, the three-byte header determines how many
bytes remain.

Known request frames:

| Operation | Hex request |
|---|---|
| Read firmware | `01 00 03 01 60` |
| Read serial number | `01 03 04 33 21` |
| Start measurement | `02 07 01 01 0D 70` |
| Read state | `01 08 01 00 E6` |
| Read measurement values | `01 11 12 5D AC` |

## CRC

The calculation is CRC-16/MODBUS: initial value `0xFFFF`, reflected polynomial
`0xA001`, XOR each byte, then shift LSB-first eight times. The SLT5006 sends the
result **high byte first**, the reverse of the usual MODBUS RTU wire order.

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

Measurement flow:

1. Write `0x01` to `0x07` and validate the complete echoed ACK.
2. Wait at least 300 ms after the ACK before the first state request.
3. Read one byte at `0x08` at intervals of at least 200 ms until bit 0 is set.
4. Stop starting new state reads after the default 30-second conversion
   deadline. A UART response timeout already in progress can add to wall time.
5. Read 18 bytes at `0x11` and convert the public values.

The start ACK means that the command was accepted, not that conversion is done.

## Data conversion

Multi-byte values are little-endian. Public values from the 18-byte block are:

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

`battery_v` is the **SLT5006-reported sensor supply** conversion. The logger's
`core2_battery_v` is the Core2 internal battery voltage and is a separate field.
Sign-extend the low 12 temperature bits before multiplying; do not interpret the
whole 16-bit word as a signed integer.

Coconut-peat pore EC is supported on SLT5006 firmware 1.7.5 and later (known
unsupported: 1.6.3; known supported: 1.7.5 and 1.7.6). Earlier firmware still
allows the complete block and word 9 to be read, but returns zero for word 9.
That zero is not a supported measurement. When support is absent or firmware is
unknown, hide the value on screen, leave its fixed CSV column empty, and omit
the JSON property.

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

Validate the error function code and CRC before accepting the error code. Log
unknown codes in hexadecimal rather than discarding them.

## Porting checklist

- Confirm every request byte was written and reconstruct split responses.
- Bound `SIZE` to 1-26 and the response to the fixed buffer.
- Validate CRC, function, address, size, and the complete write echo.
- Handle the four-byte error frame separately.
- Test high-byte-first CRC, little-endian words, and signed 12-bit temperature.
- Enforce the 300-ms initial delay, poll interval, and measurement timeout.
- Use unsigned elapsed-time arithmetic so `millis()` wraparound is safe.
- After an error, clear stale input, wait, and retry from a new measurement
  cycle instead of repeatedly sending the same request.

See the [integration guide](m5stack-integration.md) for application-level retry,
storage, and firmware-feature handling.
