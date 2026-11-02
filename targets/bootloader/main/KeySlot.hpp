#pragma once

#include <array>
#include <cstdint>

namespace bootloader
{
    struct KeySlot
    {
        std::array<uint8_t, 64> signaturePublicKey;
        std::array<uint8_t, 16> imageEncryptionKey;
    };

    static_assert(sizeof(KeySlot) == 80);

    extern const KeySlot keySlot;
}
