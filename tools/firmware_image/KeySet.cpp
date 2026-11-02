#include "tools/firmware_image/KeySet.hpp"
#include "uECC.h"
#include <algorithm>

namespace firmware_image
{
    namespace
    {
        // micro-ecc takes its random source as a plain function, so the source in use is parked here
        const RandomSource* activeRandom = nullptr;

        int UeccRandom(uint8_t* destination, unsigned size)
        {
            return (*activeRandom)(std::span<uint8_t>{ destination, size }) ? 1 : 0;
        }

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

        activeRandom = &random;
        uECC_set_rng(&UeccRandom);
        const auto made = uECC_make_key(keys.publicKey.data(), keys.privateKey.data(), uECC_secp256r1());
        uECC_set_rng(nullptr);
        activeRandom = nullptr;

        if (made != 1)
            return std::nullopt;
        return keys;
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
