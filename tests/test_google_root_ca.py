"""Offline checks for the exact official Google trust anchors used by ESP32."""

import hashlib
from pathlib import Path
import re
import ssl
import time
import unittest

HEADER = (
    Path(__file__).resolve().parents[1]
    / "examples" / "M5Core2CloudLogger" / "GoogleRootCA.h"
)

# SHA-256 of DER certificates published at pki.goog/repo/certs/gtsr{1,4}.pem.
EXPECTED = {
    "GTS Root R1": "d947432abde7b7fa90fc2e6b59101b1280e0e1c7e4e40fa3c6887fff57a7f4cf",
    "GTS Root R4": "349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d",
}


class GoogleRootTests(unittest.TestCase):
    def test_exact_official_anchors(self):
        source = HEADER.read_text(encoding="utf-8")
        bundle = re.search(r'R"CERT\((.*?)\)CERT"', source, re.S).group(1)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.load_verify_locations(cadata=bundle)
        self.assertEqual(context.cert_store_stats()["x509_ca"], len(EXPECTED))
        hashes = {
            hashlib.sha256(cert).hexdigest()
            for cert in context.get_ca_certs(binary_form=True)
        }
        self.assertEqual(hashes, set(EXPECTED.values()))
        names = set()
        for cert in context.get_ca_certs():
            subject = dict(part[0] for part in cert["subject"])
            names.add(subject["commonName"])
            self.assertEqual(cert["subject"], cert["issuer"])
            self.assertLess(ssl.cert_time_to_seconds(cert["notBefore"]), time.time())
            self.assertGreater(ssl.cert_time_to_seconds(cert["notAfter"]), time.time())
        self.assertEqual(names, set(EXPECTED))


if __name__ == "__main__":
    unittest.main()
