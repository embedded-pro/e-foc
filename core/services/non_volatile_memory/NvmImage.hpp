#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/services/non_volatile_memory/NvmRegion.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    // An image rewritten in the field: written whole without an erase, then read back and compared.
    template<std::size_t Size>
    class NvmImage
    {
    public:
        explicit NvmImage(NvmRegion& region)
            : region(region)
        {}

        infra::ByteRange Contents()
        {
            return infra::MakeRange(written);
        }

        void Write(const infra::Function<void(NvmStatus)>& onDone)
        {
            onWritten = onDone;
            region.Write(infra::MakeConstByteRange(written), [this]
                {
                    region.Read(infra::MakeRange(readBack), [this]
                        {
                            onWritten(written == readBack ? NvmStatus::Ok : NvmStatus::HardwareFault);
                        });
                });
        }

        void Read(const infra::Function<void(infra::ConstByteRange image)>& onDone)
        {
            onRead = onDone;
            region.Read(infra::MakeRange(readBack), [this]
                {
                    onRead(infra::MakeConstByteRange(readBack));
                });
        }

    private:
        NvmRegion& region;
        std::array<uint8_t, Size> written{};
        std::array<uint8_t, Size> readBack{};
        infra::AutoResetFunction<void(NvmStatus)> onWritten;
        infra::AutoResetFunction<void(infra::ConstByteRange)> onRead;
    };
}
