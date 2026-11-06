# SLT5006 UART protocol

English | [日本語](protocol.md)

This document describes only the operations used by this library. Refer to the
current SLT5006 product documents for the complete public specification,
electrical ratings, and guaranteed conditions. Do not access non-public
registers.

## UART and frame format

The connection is one-to-one at 9600 bps, 8 data bits, no parity, one stop bit,
and no flow control. There is no device address or slave ID; the first byte is
the function.

```text
read request:   01  ADDRESS  SIZE  CRC_H  CRC_L
1-byte write:  02  ADDRESS  01  DATA  CRC_H  CRC_L
normal reply:  FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
error reply:   (REQUEST_FUNCTION | 80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE` is 1–26 bytes, making the largest normal response 31 bytes. Frames have
no newline, terminator, STX, or ETX. A receiver distinguishes a four-byte error
frame from a normal response using bit 7 of the first byte; otherwise it reads
the three-byte header to determine the remaining length.

## CRC

CRC calculation uses CRC-16/MODBUS parameters: initial value `0xFFFF`, reflected
polynomial `0xA001`, XOR each byte, and shift LSB-first eight times. Unlike the
usual MODBUS RTU wire order, SLT5006 transmits the calculated CRC **high byte
first, then low byte**.

| Operation | Known request frame |
|---|---|
| Firmware | `01 00 03 01 60` |
| Serial number | `01 03 04 33 21` |
| Start measurement | `02 07 01 01 0D 70` |
| Read state | `01 08 01 00 E6` |
| Read measurement values | `01 11 12 5D AC` |

## Information and measurement sequence

Firmware is three bytes from `0x00`; the serial number is a little-endian
`uint32` from `0x03`.

1. Write `0x01` to `0x07` and validate the echo ACK.
2. Wait at least 300 ms after that ACK.
3. Read one byte from `0x08` every 200 ms until bit 0 is set.
4. Once 30 seconds have elapsed from the start ACK, do not begin another state
   poll; report a measurement timeout. Individual UART response waits can add
   time beyond that conversion deadline.
5. Read 18 bytes from `0x11` and convert the public fields.

A start ACK confirms acceptance of the command, not measurement completion.

## Measurement values

Multi-byte measurement values are little-endian.

| Word in the `0x11` block | Public value | Conversion |
|---:|---|---|
| 1 | Supply voltage | `raw * 3.3 / 4096 / 0.5` V |
| 2 | Temperature | signed low 12 bits, `* 0.0625` degC |
| 3 | Bulk EC | `uint16 * 0.001` dS/m |
| 4 | Rock-wool VWC | `uint16 * 0.1` % |
| 5 | General-soil VWC | `uint16 * 0.1` % |
| 6 | Coco-peat VWC | `uint16 * 0.1` % |
| 8 | Pore-water EC | `uint16 * 0.001` dS/m |
| 9 | Coco-peat pore EC | `uint16 * 0.001` dS/m |

Coco-peat pore EC is supported with firmware 1.7.5 or newer. Earlier firmware
still allows the entire block, including word 9, to be read and returns zero for
word 9. Applications must not treat that zero as a supported measurement.
Unlisted words are not exposed by this library.

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

The library verifies complete request writes, response length and timeout,
maximum size, CRC, function, register, size, write echo, and the error-response
function before accepting data or an error code.
