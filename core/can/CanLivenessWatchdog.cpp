#include "core/can/CanLivenessWatchdog.hpp"

namespace can
{
    CanLivenessWatchdog::CanLivenessWatchdog(services::CanProtocolServer& server, state_machine::ControlModeStateMachine& controlMode, services::Tracer& tracer)
        : services::CanProtocolServerObserver(server)
        , controlMode(controlMode)
        , tracer(tracer)
    {}

    void CanLivenessWatchdog::Online()
    {}

    void CanLivenessWatchdog::Offline()
    {
        auto& stateMachine = controlMode.ActiveStateMachine();
        const auto& state = stateMachine.CurrentState();

        if (std::holds_alternative<state_machine::Enabled>(state))
            tracer.Trace() << "[CAN] Client lost while enabled, stopping drive";
        else if (std::holds_alternative<state_machine::Calibrating>(state) && std::get<state_machine::Calibrating>(state).externalReservation)
            tracer.Trace() << "[CAN] Client lost during its identification, stopping drive";
        else
            return;

        stateMachine.CmdEmergencyStop();
    }
}
