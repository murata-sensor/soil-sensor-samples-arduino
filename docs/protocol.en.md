# SLT5006 UART protocol

English | [日本語](protocol.md)

This document covers only the operations that this library uses. For the
complete public specification, electrical ratings, and guaranteed conditions,
see the latest SLT5006 product documentation. Do not access non-public
registers.

## UART and frame format

The UART runs at 9600 bps with 8 data bits, no parity, one stop bit, and no
flow control. Connect only one sensor to each UART. Frames have no device
address or slave ID. The first byte is the function code.

```text
read request:   01  ADDRESS  SIZE  CRC_H  CRC_L
1-byte write:  02  ADDRESS  01  DATA  CRC_H  CRC_L
normal reply:  FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
error reply:   (REQUEST_FUNCTION | 80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE` is 1–26 bytes, so a normal response is at most 31 bytes. Frames have no
newline, terminator, STX, or ETX. The receiver checks bit 7 of the first byte
to tell a four-byte error frame from a normal response. For a normal response,
it reads the three-byte header to find the remaining length.

## CRC

The CRC uses the CRC-16/MODBUS parameters: initial value `0xFFFF` and reflected
polynomial `0xA001`. XOR each data byte into the CRC, then shift LSB-first eight
times. The SLT5006 sends the calculated CRC **high byte first, then low byte**.
This is the reverse of the usual MODBUS RTU wire order.

| Operation | Request frame (hex) |
|---|---|
| Read firmware version | `01 00 03 01 60` |
| Read serial number | `01 03 04 33 21` |
| Start measurement | `02 07 01 01 0D 70` |
| Read state | `01 08 01 00 E6` |
| Read measurement values | `01 11 12 5D AC` |

## Information and measurement sequence

The firmware version is three bytes at `0x00`: major, minor, and revision, one
byte each. The serial number is a little-endian `uint32` at `0x03`.

1. Write `0x01` to `0x07` and check that the sensor returns a valid echo ACK.
2. Wait at least 300 ms after the ACK before the first state read.
3. Read one byte from `0x08` every 200 ms until bit 0 is set.
4. When 30 seconds (the default) have passed since the start ACK, do not start
   another state read. Report a measurement timeout instead. Individual UART
   response waits can add time beyond this deadline.
5. Read 18 bytes from `0x11` and convert the public values.

The start ACK only means that the sensor accepted the command. Step 3 confirms
that the measurement is complete.

## Measurement values

All multi-byte values are little-endian (low byte first).

| Word in the `0x11` block | Public value | Conversion |
|---:|---|---|
| 1 | Supply voltage | `raw * 3.3 / 4096 / 0.5` V (`raw` is the internal ADC value and is not exposed) |
| 2 | Temperature | signed low 12 bits, `* 0.0625` degC |
| 3 | Bulk EC | `uint16 * 0.001` dS/m |
| 4 | Rock-wool VWC | `uint16 * 0.1` % |
| 5 | General-soil VWC | `uint16 * 0.1` % |
| 6 | Coco-peat VWC | `uint16 * 0.1` % |
| 8 | Pore-water EC | `uint16 * 0.001` dS/m |
| 9 | Coco-peat pore EC | `uint16 * 0.001` dS/m |

Coco-peat pore EC requires SLT5006 firmware 1.7.5 or later. Older firmware still
lets you read the whole block, including word 9, but word 9 is zero. Do not
treat that zero as a measurement. This library does not use or expose words
that are not listed in the table.

## Sensor errors and receiver validation

| Code | Meaning |
|---:|---|
| `0x01` | Illegal function code |
| `0x02` | Illegal start address |
| `0x03` | Illegal byte size |
| `0x04` | Internal receive-buffer overflow |
| `0x05` | CRC-16 error |
| `0x06` | Data read during measurement |
| `0x10` | Internal-storage write failure |
| `0x20` | Internal sensor communication error |
| `0x40` | Sensor-side measurement timeout |

The library validates every response before it accepts data or an error code.
It checks that the whole request was written, that the full response arrived
within the timeout, and that `SIZE` is 26 or less and fits the receive buffer.
For a normal response, it checks the CRC, function code, register address, and
size. For a write ACK, it also checks the echoed data. For an error response,
it checks the CRC and the `REQUEST_FUNCTION | 0x80` function code. Log unknown
error codes in hexadecimal.
