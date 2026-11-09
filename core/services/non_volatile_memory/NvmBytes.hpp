#pragma once

#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/Endian.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace services::nvm
{
    template<class T>
    void Put(infra::ByteRange image, std::size_t offset, T value)
    {
        really_assert(offset + sizeof(T) <= image.size());
        const infra::LittleEndian<T> stored{ value };
        std::ranges::copy(infra::MakeByteRange(stored), image.begin() + offset);
    }

    template<class T>
    T Get(infra::ConstByteRange image, std::size_t offset)
    {
        really_assert(offset + sizeof(T) <= image.size());
        infra::LittleEndian<T> stored;
        std::ranges::copy_n(image.begin() + offset, sizeof(T), infra::MakeByteRange(stored).begin());
        return stored;
    }

    inline uint32_t Crc32(infra::ConstByteRange bytes)
    {
        infra::Crc32 crc;
        crc.Update(bytes);
        return crc.Result();
    }
}
