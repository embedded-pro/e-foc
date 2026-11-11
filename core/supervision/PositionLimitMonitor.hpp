#pragma once

#include "core/foc/math/TravelLimits.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    class PositionLimitMonitor
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };

        struct Settings
        {
            foc::TravelLimits limits;
            foc::Radians margin;
        };

        // Returns the active drive while position mode is the active mode, nullptr otherwise.
        using PositionDrive = infra::Function<const state_machine::FocStateMachineBase*()>;

        explicit PositionLimitMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Settings()>& settings);
        void Attach(const PositionDrive& positionDrive, const infra::Function<foc::Radians()>& measuredPosition);

    private:
        void Evaluate();
        bool OutsideLimits() const;

        infra::Function<void(state_machine::FaultCode)> onFault;
        PositionDrive positionDrive;
        infra::Function<foc::Radians()> measuredPosition;
        infra::Function<Settings()> settings;
        infra::TimerRepeating evaluationTimer;
        bool reported{ false };
    };

    PositionLimitMonitor::Settings PositionLimitSettingsFrom(const services::ConfigData& config);
}
