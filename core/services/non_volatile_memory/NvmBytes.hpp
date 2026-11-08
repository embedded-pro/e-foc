#pragma once

#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace services::nvm
{
    template<class T>
    void Put(infra::ByteRange image, std::size_t offset, T value)
    {
        using Unsigned = std::make_unsigned_t<T>;
        for (std::size_t byte = 0; byte != sizeof(T); ++byte)
            image[offset + byte] = static_cast<uint8_t>(static_cast<Unsigned>(value) >> (8 * byte));
    }

    template<class T>
    T Get(infra::ConstByteRange image, std::size_t offset)
    {
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned value{ 0 };
        for (std::size_t byte = 0; byte != sizeof(T); ++byte)
            value |= static_cast<Unsigned>(static_cast<Unsigned>(image[offset + byte]) << (8 * byte));
        return static_cast<T>(value);
    }

    inline uint32_t Crc32(infra::ConstByteRange bytes)
    {
        infra::Crc32 crc;
        crc.Update(bytes);
        return crc.Result();
    }
}
