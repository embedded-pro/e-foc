#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/supervision/ControlHealth.hpp"
#include "infra/timer/Timer.hpp"
#include "services/tracer/Tracer.hpp"
#include <chrono>

namespace supervision
{
    class WatchdogSupervisor
    {
    public:
        struct Config
        {
            std::chrono::microseconds deadline;
            std::chrono::microseconds startupGrace;
            uint32_t evaluationsPerDeadline{ 4 };
        };

        WatchdogSupervisor(application::PlatformFactory& hardware, ControlHealth& health, services::Tracer& tracer);

        void Enable(const Config& config);
        void AttachControlMode(state_machine::ControlModeStateMachine& controlMode);

        bool IsSupervising() const;

    private:
        void Evaluate();
        bool IsEligible();
        void OnDeadlineMissed();

        application::PlatformFactory& hardware;
        ControlHealth& health;
        services::Tracer& tracer;

        state_machine::ControlModeStateMachine* controlMode{ nullptr };
        infra::TimerRepeating evaluationTimer;
        uint32_t startupEvaluationsLeft{ 0 };
    };
}
