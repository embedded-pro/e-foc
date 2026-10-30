#pragma once

#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/PlatformDiagnostics.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace supervision
{
    class ControlLoopStallMonitor
    {
    public:
        static constexpr infra::Duration checkPeriod{ std::chrono::milliseconds(1) };
        // Timer time advances in whole milliseconds and a check can run late within one, so two of them are
        // the least that proves more than 1 ms without a completed interrupt.
        static constexpr infra::Duration stallTime{ std::chrono::milliseconds(2) };

        ControlLoopStallMonitor(application::DecimatingThreePhaseInverter& stage, const application::PlatformDiagnostics& diagnostics,
            const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Check(infra::TimePoint now);

    private:
        application::DecimatingThreePhaseInverter& stage;
        const application::PlatformDiagnostics& diagnostics;
        infra::Function<void(state_machine::FaultCode)> onFault;
        std::optional<uint32_t> armedStart;
        uint32_t lastCompleted{ 0 };
        infra::TimePoint lastMoved{};
        infra::TimerRepeating checkTimer;
    };
}
