#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/supervision/SustainedCondition.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    class TargetReachedMonitor
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };

        struct Settings
        {
            foc::Radians window;
            infra::Duration time;
        };

        using PositionDrive = infra::Function<const state_machine::FocStateMachineBase*()>;

        void Enable(const infra::Function<Settings()>& settings);
        void Attach(const PositionDrive& positionDrive);
        bool IsReached() const;

    private:
        void Evaluate();
        bool WithinWindow() const;

        PositionDrive positionDrive;
        infra::Function<Settings()> settings;
        infra::TimerRepeating evaluationTimer;
        SustainedCondition within;
        bool reached{ false };
    };

    TargetReachedMonitor::Settings TargetReachedSettingsFrom(const services::ConfigData& config);
}
