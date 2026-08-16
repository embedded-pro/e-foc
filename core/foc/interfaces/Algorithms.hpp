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

    enum class PositionAlgorithm : uint8_t
    {
        pid = 0,
        cascadeP = 1,
        lqr = 2,
        lqi = 3,
        twoDof = 4
    };

    // Position algorithms either drive the speed loop or command torque directly.
    enum class PositionOutputKind : uint8_t
    {
        speedReference = 0,
        currentReference = 1
    };

    enum class SelectResult : uint8_t
    {
        ok = 0,
        busy = 1,
        invalidAlgorithm = 2,
        invalidParameters = 3
    };
}
