#!/usr/bin/env python3
"""Writes data/cert/x509_crt_bundle.bin: the trusted root CAs WiFiClientSecure::setCACertBundle() checks
HTTPS servers against (GitHub Pages for updates, and anything else the firmware fetches over TLS).

Format (Arduino-ESP32 2.x esp_crt_bundle.c): a big-endian u16 certificate count, then per certificate a
u16 subject length, u16 public key length, the DER subject name and the DER SubjectPublicKeyInfo, sorted
by subject bytes (the device binary-searches it). No dependencies: the two fields are cut straight out of
each certificate's DER.

Usage: python3 -I tools/gen_crt_bundle.py [cacert.pem]
The default input is certifi's Mozilla bundle (pip install certifi). Rerun now and then to pick up new roots.
"""
import base64
import pathlib
import re
import struct
import sys

OUT = pathlib.Path(__file__).resolve().parent.parent / "data/cert/x509_crt_bundle.bin"


def tlv(der, pos):
    """Returns (start of value, end of value, end of whole element) for the element at pos."""
    length = der[pos + 1]
    start = pos + 2
    if length & 0x80:
        n = length & 0x7F
        length = int.from_bytes(der[start:start + n], "big")
        start += n
    return start, start + length, start + length


def subject_and_key(der):
    _, cert_end, _ = tlv(der, 0)               # Certificate
    tbs_start, _, _ = tlv(der, tlv(der, 0)[0])  # TBSCertificate
    fields = []
    pos = tbs_start
    while len(fields) < 7:
        _, _, end = tlv(der, pos)
        fields.append((der[pos], der[pos:end]))
        pos = end
    if fields[0][0] == 0xA0:  # explicit [0] version is present
        fields = fields[1:]
    # serial, signature, issuer, validity, subject, subjectPublicKeyInfo
    return fields[4][1], fields[5][1]


def main():
    if len(sys.argv) > 1:
        pem = pathlib.Path(sys.argv[1]).read_text()
    else:
        import certifi
        pem = pathlib.Path(certifi.where()).read_text()

    certs = []
    for block in re.findall(r"-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----", pem, re.S):
        certs.append(subject_and_key(base64.b64decode("".join(block.split()))))
    certs.sort(key=lambda c: c[0])

    out = bytearray(struct.pack(">H", len(certs)))
    for subject, key in certs:
        out += struct.pack(">HH", len(subject), len(key)) + subject + key
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(out)
    print(f"{len(certs)} root certificates, {len(out)} bytes -> {OUT}")


if __name__ == "__main__":
    main()
