"""Derive the e-foc development keys and print them as a C++ header.

The development keys are public on purpose: anyone can sign and encrypt a pack
that a development bootloader accepts. They are derived from fixed seeds, so the
repository holds no private key file, and the pack tooling derives the same
private key from the same seed. Release bootloaders use other keys.
"""

import argparse
import hashlib

from cryptography.hazmat.primitives.asymmetric import ec

SIGNING_SEED = b"e-foc development signing key"
ENCRYPTION_SEED = b"e-foc development encryption key"


def signing_key():
    order = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
    scalar = int.from_bytes(hashlib.sha256(SIGNING_SEED).digest(), "big") % order
    return ec.derive_private_key(scalar, ec.SECP256R1())


def encryption_key():
    return hashlib.sha256(ENCRYPTION_SEED).digest()[:16]


def public_key_bytes(private_key):
    numbers = private_key.public_key().public_numbers()
    return numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")


def array(values):
    return "\n".join(f"        0x{value:02x}," for value in values)


def header():
    public_key = public_key_bytes(signing_key())
    aes_key = encryption_key()
    return f"""#pragma once

#include <array>
#include <cstdint>

namespace bootloader::keys
{{
    inline constexpr std::array<uint8_t, {len(public_key)}> signaturePublicKey{{ {{
{array(public_key)}
    }} }};

    inline constexpr std::array<uint8_t, {len(aes_key)}> imageEncryptionKey{{ {{
{array(aes_key)}
    }} }};
}}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", help="header file to write")
    arguments = parser.parse_args()
    with open(arguments.output, "w", encoding="utf-8", newline="\n") as file:
        file.write(header())


if __name__ == "__main__":
    main()
