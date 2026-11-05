#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>

namespace supervision
{
    class OverspeedMonitor
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };
        static constexpr uint8_t evaluationsToFault{ 5 };
        static constexpr float faultMargin{ 1.1f };

        explicit OverspeedMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<foc::RadiansPerSecond()>& speedLimit);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();
        bool BeyondLimit(const state_machine::FocStateMachineBase& drive) const;

        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<foc::RadiansPerSecond()> speedLimit;
        infra::TimerRepeating evaluationTimer;
        uint8_t evaluationsBeyond{ 0 };
        bool reported{ false };
    };
}
