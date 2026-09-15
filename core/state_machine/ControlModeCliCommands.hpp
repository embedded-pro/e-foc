#pragma once

#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace state_machine
{
    class ControlModeStateMachine;

    void RegisterControlModeCliCommands(
        services::TerminalWithStorage& terminal,
        ControlModeStateMachine& sm,
        services::Tracer& tracer);
}
