#include "tools/firmware_image/KeySet.hpp"
#include "uECC.h"
#include <algorithm>

namespace firmware_image
{
    namespace
    {
        constexpr std::size_t privateKeyDraws = 64;

        template<std::size_t N>
        void CopyOut(std::span<const uint8_t> from, std::size_t offset, std::array<uint8_t, N>& to)
        {
            std::copy_n(from.begin() + static_cast<std::ptrdiff_t>(offset), N, to.begin());
        }
    }

    std::optional<KeySet> GenerateKeySet(const RandomSource& random)
    {
        KeySet keys{};
        if (!random(keys.aesKey))
            return std::nullopt;

        // A draw outside [1, n - 1] is refused by uECC_compute_public_key and drawn again, as uECC_make_key does
        for (std::size_t draw = 0; draw < privateKeyDraws; ++draw)
        {
            if (!random(keys.privateKey))
                return std::nullopt;
            if (uECC_compute_public_key(keys.privateKey.data(), keys.publicKey.data(), uECC_secp256r1()) == 1)
                return keys;
        }

        return std::nullopt;
    }

    std::vector<uint8_t> Serialize(const KeySet& keys)
    {
        std::vector<uint8_t> file;
        file.reserve(keySetFileSize);
        file.insert(file.end(), keys.aesKey.begin(), keys.aesKey.end());
        file.insert(file.end(), keys.publicKey.begin(), keys.publicKey.end());
        file.insert(file.end(), keys.privateKey.begin(), keys.privateKey.end());
        return file;
    }

    std::variant<KeySet, KeySetError> ParseKeySet(std::span<const uint8_t> file)
    {
        if (file.size() != keySetFileSize)
            return KeySetError::wrongSize;

        KeySet keys{};
        CopyOut(file, 0, keys.aesKey);
        CopyOut(file, 16, keys.publicKey);
        CopyOut(file, 80, keys.privateKey);

        std::array<uint8_t, 64> computed{};
        if (uECC_compute_public_key(keys.privateKey.data(), computed.data(), uECC_secp256r1()) != 1 || computed != keys.publicKey)
            return KeySetError::publicKeyMismatch;

        return keys;
    }

    std::array<uint8_t, bootloaderKeySlotSize> BootloaderKeySlot(const KeySet& keys)
    {
        std::array<uint8_t, bootloaderKeySlotSize> slot{};
        std::ranges::copy(keys.publicKey, slot.begin());
        std::ranges::copy(keys.aesKey, slot.begin() + keys.publicKey.size());
        return slot;
    }
}
