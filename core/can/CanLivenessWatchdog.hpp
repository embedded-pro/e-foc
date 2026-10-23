#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "services/tracer/Tracer.hpp"

namespace can
{
    class CanLivenessWatchdog
        : public services::CanProtocolServerObserver
    {
    public:
        CanLivenessWatchdog(services::CanProtocolServer& server, state_machine::ControlModeStateMachine& controlMode, services::Tracer& tracer);

        void Online() override;
        void Offline() override;

    private:
        state_machine::ControlModeStateMachine& controlMode;
        services::Tracer& tracer;
    };
}
