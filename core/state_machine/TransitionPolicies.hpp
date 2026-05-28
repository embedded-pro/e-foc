#pragma once

#include <cstdint>

namespace state_machine
{
    // Controls whether FocStateMachineImpl registers state-transition commands on the CLI terminal.
    // Cli:  CmdCalibrate/CmdEnable/CmdDisable/etc. are registered as terminal commands.
    // Auto: no CLI commands are registered; the caller invokes transition methods directly.
    enum class TransitionPolicy : uint8_t
    {
        Cli,
        Auto
    };
}
