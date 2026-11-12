#pragma once

#include "core/services/leds/LedStatusSource.hpp"
#include "services/peripheral/LedPattern.hpp"

namespace services
{
    struct LedPatterns
    {
        LedPattern green;
        LedPattern amber;
        LedPattern red;
    };

    LedPatterns PatternsFor(const LedStatus& status);
}
