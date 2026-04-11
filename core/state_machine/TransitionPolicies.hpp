#pragma once

namespace state_machine
{
    // Tag struct: transitions are driven by CLI terminal commands.
    // FocStateMachineImpl registers Cmd* methods as terminal commands in CLI mode.
    struct CliTransitionPolicy
    {};

    // Tag struct: transitions are driven programmatically via CmdCalibrate(), CmdEnable(), etc.
    // No terminal commands are registered. The caller is responsible for invoking transition
    // methods at the appropriate time (e.g., from CAN commands or automated sequences).
    struct AutoTransitionPolicy
    {};
}
