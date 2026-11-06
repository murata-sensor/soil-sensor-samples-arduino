import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch
import warnings

SPEC = importlib.util.spec_from_file_location(
    "configure_logger",
    Path(__file__).resolve().parents[1] / "scripts" / "configure_logger.py"
)
logger = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(logger)


class FakeSerial:
    def __init__(self, responses=(), short_write=False):
        self.responses = list(responses)
        self.writes = []
        self.now = 0
        self.short_write = short_write

    def clock(self):
        return self.now

    def read(self, size):
        self.now += 0.1
        return self.responses.pop(0) if self.responses else b""

    def write(self, data):
        self.writes.append(data)
        return len(data) - 1 if self.short_write else len(data)


class ConfigTests(unittest.TestCase):
    def test_hidden_input_refuses_echo_fallback(self):
        def unavailable(prompt):
            warnings.warn("echo fallback", logger.getpass.GetPassWarning)
            self.fail("Input must stop before echo fallback")

        with patch.object(logger.getpass, "getpass", unavailable):
            with self.assertRaises(logger.getpass.GetPassWarning):
                logger.hidden_input("secret")

    def test_defaults_and_unicode(self):
        self.assertEqual(logger.encode_config({}), b"{}")
        config = {"wifi_ssid": "日本語", "wifi_password": 'secret"\\\n'}
        self.assertEqual(json.loads(logger.encode_config(config)), config)

    def test_invalid_values(self):
        cases = [
            [], {"unknown": "secret"}, {"wifi_ssid": None},
            {"wifi_ssid": "日" * 11}, {"wifi_password": "x" * 65},
            {"gas_token": "x" * 257}, {"timezone": ""},
            {"wifi_ssid": "a\0b"}, {"sample_interval_sec": "60"},
            {"sample_interval_sec": True}, {"sample_interval_sec": 60.0},
            {"sample_interval_sec": 4}, {"sample_interval_sec": 86401},
            {"upload_batch_size": 0}, {"upload_batch_size": 21},
            {"deep_sleep_sec": 9}, {"deep_sleep_sec": -1},
            {"deep_sleep_sec": 86401}, {"gas_url": "http://example"},
            {"gas_url": "https://example", "gas_token": "token"},
            {"wifi_ssid": "wifi", "gas_url": "https://example"},
            {"wifi_ssid": "wifi", "gas_token": "token"},
        ]
        for config in cases:
            with self.subTest(config=config), self.assertRaises(ValueError):
                logger.encode_config(config)

    def test_boundaries_and_full_cloud_config(self):
        for sleep in (0, 10, 86400):
            config = {
                "wifi_ssid": "w" * 32, "wifi_password": "p" * 64,
                "gas_url": "https://" + "u" * 504, "gas_token": "t" * 256,
                "ntp_server": "n" * 253, "timezone": "z" * 128,
                "sample_interval_sec": 86400, "upload_batch_size": 20,
                "deep_sleep_sec": sleep,
            }
            self.assertEqual(json.loads(logger.encode_config(config)), config)
        logger.encode_config({"sample_interval_sec": 5, "upload_batch_size": 1})

    def test_payload_limit_is_bytes(self):
        with self.assertRaises(ValueError):
            logger.encode_config({
                "wifi_ssid": "w", "wifi_password": "\n" * 64,
                "gas_url": "https://" + "\n" * 504,
                "gas_token": "\n" * 256, "ntp_server": "\n" * 253,
                "timezone": "\n" * 128,
            })

    def test_duplicate_keys(self):
        with self.assertRaises(ValueError):
            json.loads('{"wifi_ssid":"a","wifi_ssid":"b"}',
                       object_pairs_hook=logger.unique_object)

    def test_fragmented_ack_and_unrelated_logs(self):
        connection = FakeSerial([
            b'{"measurement":1}\n[CON', b"FIG] READY\r\n",
            b"[CONFIG] READY\nnoise\n[CONFIG] SA", b"VED\r\n",
        ])
        logger.send_config(connection, b"{}", clock=connection.clock)
        self.assertEqual(connection.writes, [b"CONFIG BEGIN\n", b"{}\n"])

    def test_error_does_not_echo_secret(self):
        connection = FakeSerial([b"[CONFIG] READY\n",
                                 b"[CONFIG] ERROR secret-value\n"])
        with self.assertRaises(RuntimeError) as error:
            logger.send_config(connection, b"{}", clock=connection.clock)
        self.assertNotIn("secret-value", str(error.exception))

    def test_retries_begin_but_never_payload(self):
        connection = FakeSerial([b""] * 12 + [b"[CONFIG] READY\n"])
        with self.assertRaises(TimeoutError):
            logger.send_config(connection, b"{}", clock=connection.clock)
        self.assertGreater(connection.writes.count(b"CONFIG BEGIN\n"), 1)
        self.assertEqual(connection.writes.count(b"{}\n"), 1)

    def test_no_ready_or_unsolicited_saved_is_not_success(self):
        connection = FakeSerial([b"[CONFIG] SAVED\n"])
        with self.assertRaises(TimeoutError):
            logger.send_config(connection, b"{}", timeout=2, clock=connection.clock)
        self.assertNotIn(b"{}\n", connection.writes)

    def test_short_write(self):
        connection = FakeSerial(short_write=True)
        with self.assertRaises(OSError):
            logger.send_config(connection, b"{}", clock=connection.clock)

    def test_oversized_response(self):
        connection = FakeSerial([b"x" * 4097])
        with self.assertRaises(RuntimeError):
            logger.send_config(connection, b"{}", clock=connection.clock)


if __name__ == "__main__":
    unittest.main()
