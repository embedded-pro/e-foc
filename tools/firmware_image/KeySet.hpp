#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace firmware_image
{
    inline constexpr std::size_t keySetFileSize = 112;
    inline constexpr std::size_t bootloaderKeySlotSize = 80;

    using RandomSource = std::function<bool(std::span<uint8_t> bytes)>;

    struct KeySet
    {
        std::array<uint8_t, 16> aesKey;
        std::array<uint8_t, 64> publicKey;
        std::array<uint8_t, 32> privateKey;

        bool operator==(const KeySet& other) const = default;
    };

    enum class KeySetError
    {
        wrongSize,
        publicKeyMismatch,
    };

    std::optional<KeySet> GenerateKeySet(const RandomSource& random);
    std::vector<uint8_t> Serialize(const KeySet& keys);
    std::variant<KeySet, KeySetError> ParseKeySet(std::span<const uint8_t> file);
    std::array<uint8_t, bootloaderKeySlotSize> BootloaderKeySlot(const KeySet& keys);
}
