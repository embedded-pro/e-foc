#include "core/supervision/BusVoltageLimitMonitor.hpp"
#include <variant>

namespace supervision
{
    BusVoltageLimitMonitor::BusVoltageLimitMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault)
        : onFault{ onFault }
    {}

    void BusVoltageLimitMonitor::Enable(const infra::Function<Settings()>& settings)
    {
        this->settings = settings;
    }

    void BusVoltageLimitMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void BusVoltageLimitMonitor::Evaluate(foc::Volts bus)
    {
        if (activeDrive == nullptr || settings == nullptr)
            return;

        if (!std::holds_alternative<state_machine::Enabled>(activeDrive().CurrentState()))
        {
            reported = false;
            return;
        }

        const auto violation = Violation(bus);
        if (reported || !violation.has_value())
            return;

        reported = true;
        onFault(*violation);
    }

    std::optional<state_machine::FaultCode> BusVoltageLimitMonitor::Violation(foc::Volts bus) const
    {
        const auto limits = settings();

        if (limits.minimum.Value() > 0.0f && bus.Value() < limits.minimum.Value())
            return state_machine::FaultCode::undervoltage;
        if (limits.maximum.Value() > 0.0f && bus.Value() > limits.maximum.Value())
            return state_machine::FaultCode::overvoltage;
        return std::nullopt;
    }

    BusVoltageLimitMonitor::Settings BusVoltageLimitSettingsFrom(const services::ConfigData& config)
    {
        return { foc::Volts{ config.limits.busVoltageMinimum }, foc::Volts{ config.limits.busVoltageMaximum } };
    }
}
