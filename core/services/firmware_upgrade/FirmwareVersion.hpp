#pragma once

#include <cstdint>

namespace services
{
    struct FirmwareVersion
    {
        uint8_t major{};
        uint8_t minor{};
        uint8_t patch{};

        bool operator==(const FirmwareVersion& other) const = default;
    };
}
