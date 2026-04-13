#pragma once

#include "infra/stream/OutputStream.hpp"
#include <cstdint>

namespace application
{
    enum class ResetCause : uint8_t
    {
        powerUp = 0,
        brownOut = 1,
        software = 2,
        hardware = 3,
        watchdog = 4,
    };

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, ResetCause cause);
}
