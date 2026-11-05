#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

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

        // Returns the active drive while position mode is the active mode, nullptr otherwise.
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
        std::optional<infra::TimePoint> withinSince;
        bool reached{ false };
    };

    TargetReachedMonitor::Settings TargetReachedSettingsFrom(const services::ConfigData& config);
}
