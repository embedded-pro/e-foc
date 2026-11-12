#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NvmBytes.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>

namespace services
{
    template<class T, uint32_t Magic, uint8_t Version>
    struct NvmRecord
    {
        static_assert(std::is_trivially_copyable_v<T>);

        static constexpr std::size_t versionOffset{ 4 };
        static constexpr std::size_t crcOffset{ 5 };
        static constexpr std::size_t dataOffset{ 9 };
        static constexpr std::size_t size{ dataOffset + sizeof(T) };

        static void Serialize(const T& data, infra::ByteRange image)
        {
            really_assert(image.size() == size);

            const auto bytes = infra::MakeConstByteRange(data);
            nvm::Put<uint32_t>(image, 0, Magic);
            image[versionOffset] = Version;
            nvm::Put<uint32_t>(image, crcOffset, nvm::Crc32(bytes));
            std::memcpy(image.begin() + dataOffset, bytes.begin(), sizeof(T));
        }

        static std::optional<T> Deserialize(infra::ConstByteRange image)
        {
            really_assert(image.size() == size);

            const auto bytes = infra::DiscardHead(image, dataOffset);
            if (nvm::Get<uint32_t>(image, 0) != Magic || image[versionOffset] != Version || nvm::Get<uint32_t>(image, crcOffset) != nvm::Crc32(bytes))
                return std::nullopt;

            T data;
            std::memcpy(&data, bytes.begin(), sizeof(T));
            return data;
        }
    };

    using CalibrationRecord = NvmRecord<CalibrationData, CalibrationMagic, CalibrationLayoutVersion>;
    using ConfigRecord = NvmRecord<ConfigData, ConfigMagic, ConfigLayoutVersion>;
}
