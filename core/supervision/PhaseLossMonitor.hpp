#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    class PhaseLossMonitor
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(10) };
        static constexpr infra::Duration window{ std::chrono::milliseconds(50) };
        static constexpr float minimumDemandFraction{ 0.1f };
        static constexpr float missingCurrentFraction{ 0.2f };
        static constexpr float slowSpeedRadiansPerSecond{ 10.0f };

        PhaseLossMonitor(foc::Ampere maxCurrent, const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable();
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();
        bool CurrentMissing(const foc::MotionObservation& observation) const;

        foc::Ampere maxCurrent;
        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::TimerRepeating evaluationTimer;
        infra::Duration missingFor{};
        bool reported{ false };
    };
}
