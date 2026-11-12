#pragma once

#include "core/state_machine/FocStateMachine.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <chrono>
#include <cstddef>

namespace supervision
{
    class PeakCurrent
    {
    public:
        static constexpr infra::Duration samplePeriod{ std::chrono::milliseconds(1) };
        static constexpr std::size_t window{ 10 };

        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

        foc::Ampere Peak() const;

    private:
        void Sample();

        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::TimerRepeating sampleTimer;
        std::array<float, window> samples{};
        std::size_t next{ 0 };
    };
}
