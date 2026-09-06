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

        if (!std::holds_alternative<state_machine::Enabled>(stateMachine.CurrentState()))
            return;

        tracer.Trace() << "[CAN] Client lost while enabled, stopping drive";
        stateMachine.CmdEmergencyStop();
    }
}
