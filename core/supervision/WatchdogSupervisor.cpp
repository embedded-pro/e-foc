#include "core/supervision/WatchdogSupervisor.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace supervision
{
    WatchdogSupervisor::WatchdogSupervisor(application::PlatformFactory& hardware, ControlHealth& health, services::Tracer& tracer)
        : hardware{ hardware }
        , health{ health }
        , tracer{ tracer }
    {}

    void WatchdogSupervisor::Enable(const Config& config)
    {
        really_assert(config.evaluationsPerDeadline > 0);
        really_assert(config.deadline > std::chrono::microseconds::zero());
        // A negative grace would divide to a negative count and wrap on conversion, leaving startup
        // supervised in name only
        really_assert(config.startupGrace >= std::chrono::microseconds::zero());

        const auto evaluationPeriod = config.deadline / config.evaluationsPerDeadline;
        really_assert(evaluationPeriod > std::chrono::microseconds::zero());

        startupEvaluationsLeft = static_cast<uint32_t>(config.startupGrace / evaluationPeriod);

        hardware.Watchdog().Enable(config.deadline, [this]()
            {
                OnDeadlineMissed();
            });

        evaluationTimer.Start(std::chrono::duration_cast<infra::Duration>(evaluationPeriod), [this]()
            {
                Evaluate();
            });
    }

    void WatchdogSupervisor::AttachControlMode(state_machine::ControlModeStateMachine& controlMode)
    {
        this->controlMode = &controlMode;
    }

    bool WatchdogSupervisor::IsSupervising() const
    {
        return hardware.Watchdog().IsEnabled();
    }

    void WatchdogSupervisor::Evaluate()
    {
        if (IsEligible())
            hardware.Watchdog().Feed();
    }

    bool WatchdogSupervisor::IsEligible()
    {
        if (controlMode == nullptr)
        {
            // Startup is graced, not exempted: a boot that never reaches an attached control mode stops
            // feeding and lets the deadline expire
            if (startupEvaluationsLeft == 0)
                return false;

            --startupEvaluationsLeft;
            return true;
        }

        return health.TakeEligibility(controlMode->Active(), controlMode->ActiveStateMachine().CurrentState());
    }

    void WatchdogSupervisor::OnDeadlineMissed()
    {
        evaluationTimer.Cancel();

        if (controlMode != nullptr)
            controlMode->ActiveStateMachine().CmdEmergencyStop();

        hardware.Stop();
        tracer.Trace() << "[WDT] deadline missed, power stage stopped";

        hardware.ResetFromWatchdogExpiry();
    }
}
