#pragma once

#include "core/services/leds/LedStatusSource.hpp"
#include <cstdint>

namespace services
{
    class LedPattern
    {
    public:
        static constexpr LedPattern Off()
        {
            return LedPattern{ Kind::off, 0, 0, 0, 0 };
        }

        static constexpr LedPattern Steady()
        {
            return LedPattern{ Kind::steady, 0, 0, 0, 0 };
        }

        static constexpr LedPattern Flashes(uint8_t count, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
        {
            return LedPattern{ Kind::flashes, count, onTicks, offTicks, pauseTicks };
        }

        bool LevelAt(uint32_t tick) const;
        uint32_t PeriodTicks() const;

        bool operator==(const LedPattern&) const = default;

    private:
        enum class Kind : uint8_t
        {
            off,
            steady,
            flashes
        };

        constexpr LedPattern(Kind kind, uint8_t flashes, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
            : kind{ kind }
            , flashes{ flashes }
            , onTicks{ onTicks }
            , offTicks{ offTicks }
            , pauseTicks{ pauseTicks }
        {}

        Kind kind;
        uint8_t flashes;
        uint16_t onTicks;
        uint16_t offTicks;
        uint16_t pauseTicks;
    };

    struct LedPatterns
    {
        LedPattern green;
        LedPattern amber;
        LedPattern red;
    };

    LedPatterns PatternsFor(const LedStatus& status);
}
