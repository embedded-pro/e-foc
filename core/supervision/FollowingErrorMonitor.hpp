#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/supervision/SustainedCondition.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    class FollowingErrorMonitor
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };

        struct Settings
        {
            foc::Radians window;
            infra::Duration time;
        };

        explicit FollowingErrorMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Settings()>& settings);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();

        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<Settings()> settings;
        infra::TimerRepeating evaluationTimer;
        SustainedCondition beyond;
    };

    FollowingErrorMonitor::Settings FollowingErrorSettingsFrom(const services::ConfigData& config);
}
