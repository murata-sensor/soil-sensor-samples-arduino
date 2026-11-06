#!/usr/bin/env python3
"""Provision M5Core2CloudLogger over USB Serial without writing to microSD."""

from __future__ import annotations

import argparse
import getpass
import json
import sys
import time
import warnings
from pathlib import Path

MAX_JSON_BYTES = 2048
STRING_LIMITS = {
    "wifi_ssid": 32,
    "wifi_password": 64,
    "gas_url": 512,
    "gas_token": 256,
    "ntp_server": 253,
    "timezone": 128,
}
NUMBER_LIMITS = {
    "sample_interval_sec": (5, 86400),
    "upload_batch_size": (1, 20),
    "deep_sleep_sec": (0, 86400),
}


def encode_config(config: object) -> bytes:
    if not isinstance(config, dict):
        raise ValueError("Configuration must be a JSON object.")
    for key, value in config.items():
        if key in STRING_LIMITS:
            if (
                not isinstance(value, str)
                or "\0" in value
                or len(value.encode("utf-8")) > STRING_LIMITS[key]
                or (key in ("ntp_server", "timezone") and not value)
            ):
                raise ValueError("Invalid configuration string.")
        elif key in NUMBER_LIMITS:
            minimum, maximum = NUMBER_LIMITS[key]
            if type(value) is not int or not minimum <= value <= maximum:
                raise ValueError("Invalid configuration number.")
            if key == "deep_sleep_sec" and 0 < value < 10:
                raise ValueError("Deep sleep must be 0 or 10-86400 seconds.")
        else:
            raise ValueError("Unknown configuration key.")
    url = config.get("gas_url", "")
    token = config.get("gas_token", "")
    if (
        (url and not url.startswith("https://"))
        or bool(url) != bool(token)
        or (url and not config.get("wifi_ssid"))
    ):
        raise ValueError("Cloud requires Wi-Fi SSID, HTTPS URL and token.")
    payload = json.dumps(config, ensure_ascii=False, separators=(",", ":")).encode(
        "utf-8"
    )
    if len(payload) > MAX_JSON_BYTES:
        raise ValueError("Configuration exceeds 2048 bytes.")
    return payload


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate configuration key.")
        result[key] = value
    return result


def hidden_input(prompt: str) -> str:
    with warnings.catch_warnings():
        warnings.simplefilter("error", getpass.GetPassWarning)
        return getpass.getpass(prompt)


def prompt_config() -> dict:
    config = {
        "wifi_ssid": input("Wi-Fi SSID (empty disables Wi-Fi): "),
        "wifi_password": hidden_input("Wi-Fi password (hidden): "),
        "gas_url": hidden_input("GAS HTTPS /exec URL (hidden; empty disables cloud): "),
        "gas_token": hidden_input("GAS INGEST_TOKEN (hidden; empty disables cloud): "),
        "ntp_server": input("NTP server [pool.ntp.org]: ") or "pool.ntp.org",
        "timezone": input("POSIX timezone [JST-9]: ") or "JST-9",
    }
    for key, default in (
        ("sample_interval_sec", 60),
        ("upload_batch_size", 10),
        ("deep_sleep_sec", 0),
    ):
        raw = input(f"{key} [{default}]: ")
        try:
            config[key] = int(raw) if raw else default
        except ValueError:
            raise ValueError("Configuration numbers must be integers.") from None
    return config


def send_config(
    serial, payload: bytes, timeout: float = 90, clock=time.monotonic
) -> None:
    """Wait for READY, send once, and accept only an explicit NVS commit ACK."""
    deadline = clock() + timeout
    next_begin = clock()
    buffer = bytearray()
    sent = False
    while clock() < deadline:
        if not sent and clock() >= next_begin:
            if serial.write(b"CONFIG BEGIN\n") != len(b"CONFIG BEGIN\n"):
                raise OSError("Incomplete USB serial write.")
            next_begin = clock() + 1
        buffer.extend(serial.read(256))
        while b"\n" in buffer:
            line, _, remainder = buffer.partition(b"\n")
            buffer = bytearray(remainder)
            line = line.rstrip(b"\r")
            if line.startswith(b"[CONFIG] ERROR "):
                # Device error text and unrelated logs are deliberately not echoed.
                raise RuntimeError("Core2 rejected configuration or NVS storage failed.")
            if line == b"[CONFIG] READY" and not sent:
                if serial.write(payload + b"\n") != len(payload) + 1:
                    raise OSError("Incomplete USB serial write.")
                sent = True
                deadline = clock() + 15
            elif line == b"[CONFIG] SAVED" and sent:
                return
        if len(buffer) > 4096:
            raise RuntimeError("USB serial response line exceeds 4096 bytes.")
    raise TimeoutError(
        "No save confirmation. Check the device before retrying; "
        "if asleep or busy, reset Core2 while this tool is waiting."
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB COM port, for example COM4")
    parser.add_argument("--file", type=Path, help="Private JSON on PC; otherwise prompt")
    args = parser.parse_args()
    try:
        if args.file:
            config = json.loads(
                args.file.read_text(encoding="utf-8-sig"),
                object_pairs_hook=unique_object,
            )
        else:
            config = prompt_config()
        payload = encode_config(config)
    except (ValueError, OSError, UnicodeError):
        print("Invalid input or unreadable config file; nothing sent.", file=sys.stderr)
        return 1
    except getpass.GetPassWarning:
        print("Hidden input unavailable. Use an interactive terminal or private JSON.",
              file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("Input cancelled; nothing sent.", file=sys.stderr)
        return 1
    try:
        import serial
    except ImportError:
        print("Install pyserial: py -m pip install pyserial", file=sys.stderr)
        return 1
    try:
        # Do not intentionally reset via DTR/RTS when opening the port.
        with serial.Serial(
            port=None, baudrate=115200, timeout=0.1, write_timeout=5
        ) as connection:
            connection.dtr = False
            connection.rts = False
            connection.port = args.port
            connection.open()
            print("Waiting for Core2. If asleep/busy, press reset now. Close Serial Monitor.")
            try:
                send_config(connection, payload)
            except (RuntimeError, TimeoutError, KeyboardInterrupt):
                connection.write(b"CONFIG CANCEL\n")
                raise
    except (serial.SerialException, OSError, RuntimeError):
        print("Provisioning failed; no confirmed save. Check port, input and device.",
              file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Provisioning cancelled.", file=sys.stderr)
        return 1
    print("NVS configuration saved. Core2 will restart; no microSD config was written.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
