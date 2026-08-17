#pragma once

#include <cstdint>

namespace state_machine
{
    // Cli registers CmdCalibrate/CmdEnable/CmdDisable and friends as terminal commands;
    // Auto registers none and expects the caller to invoke the transition methods directly.
    enum class TransitionPolicy : uint8_t
    {
        Cli,
        Auto
    };
}
