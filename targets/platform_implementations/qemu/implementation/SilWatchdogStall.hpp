#pragma once

#include "infra/timer/Timer.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace application
{
    // Lets a SIL scenario stall the event loop, so the watchdog reset path runs in CI on the emulated target
    class SilWatchdogStall
        : public services::TerminalCommands
    {
    public:
        SilWatchdogStall(services::TerminalWithCommands& terminal, services::Tracer& tracer);

        infra::MemoryRange<const Command> Commands() override;

    private:
        void Stall();

        // Leaves the command's trace time to reach the host before the event loop stops
        static constexpr infra::Duration stallDelay{ std::chrono::milliseconds(100) };

        services::Tracer& tracer;
        infra::TimerSingleShot stallTimer;
        const std::array<Command, 1> commands;
    };
}
