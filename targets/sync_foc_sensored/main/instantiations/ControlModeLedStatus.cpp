#include "targets/sync_foc_sensored/main/instantiations/ControlModeLedStatus.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <variant>

namespace application
{
    namespace
    {
        services::LedLifecycle LifecycleOf(const state_machine::State& state)
        {
            if (std::holds_alternative<state_machine::Fault>(state))
                return services::LedLifecycle::fault;
            if (std::holds_alternative<state_machine::Calibrating>(state))
                return services::LedLifecycle::calibrating;
            if (std::holds_alternative<state_machine::Enabled>(state))
                return services::LedLifecycle::enabled;
            if (std::holds_alternative<state_machine::Ready>(state))
                return services::LedLifecycle::ready;
            return services::LedLifecycle::idle;
        }
    }

    ControlModeLedStatus::ControlModeLedStatus(const std::optional<state_machine::ControlModeStateMachine>& controlMode)
        : controlMode{ controlMode }
    {}

    services::LedStatus ControlModeLedStatus::Status() const
    {
        if (!controlMode)
            return services::LedStatus{};

        const auto& stateMachine = controlMode->ActiveStateMachine();
        const auto lifecycle = LifecycleOf(stateMachine.CurrentState());
        const auto fault = lifecycle == services::LedLifecycle::fault ? stateMachine.LastFaultCode() : state_machine::FaultCode::none;
        return services::LedStatus{ lifecycle, fault };
    }
}
