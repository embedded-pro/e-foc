#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/util/Function.hpp"
#include <optional>

namespace supervision
{
    class BusVoltageLimitMonitor
    {
    public:
        struct Settings
        {
            foc::Volts minimum;
            foc::Volts maximum;
        };

        explicit BusVoltageLimitMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Settings()>& settings);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);
        // Takes the bus-voltage service's filtered value; the filter is the persistence.
        void Evaluate(foc::Volts bus);

    private:
        std::optional<state_machine::FaultCode> Violation(foc::Volts bus) const;

        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<Settings()> settings;
        bool reported{ false };
    };

    BusVoltageLimitMonitor::Settings BusVoltageLimitSettingsFrom(const services::ConfigData& config);
}
