#pragma once

#include <cstdint>

namespace bootloader
{
    enum class DeviceFamily : uint16_t
    {
        tivaTm4c = 1,
        stm32 = 2,
    };

    struct ImageTarget
    {
        uint16_t boardKind;
        DeviceFamily family;
        uint32_t device;

        bool operator==(const ImageTarget& other) const = default;
    };
}
