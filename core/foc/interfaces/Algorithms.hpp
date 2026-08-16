#pragma once

#include <cstdint>

namespace foc
{
    enum class CurrentAlgorithm : uint8_t
    {
        pid = 0,
        decoupledPid = 1,
        deadbeat = 2,
        slidingMode = 3
    };

    enum class SpeedAlgorithm : uint8_t
    {
        pid = 0,
        lqi = 1,
        adrc = 2,
        twoDof = 3
    };

    enum class SelectResult : uint8_t
    {
        ok = 0,
        busy = 1,
        invalidAlgorithm = 2,
        invalidParameters = 3
    };
}
