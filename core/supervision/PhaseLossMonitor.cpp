#include "core/supervision/PhaseLossMonitor.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    PhaseLossMonitor::PhaseLossMonitor(foc::Ampere maxCurrent, const infra::Function<void(state_machine::FaultCode)>& onFault)
        : maxCurrent{ maxCurrent }
        , onFault{ onFault }
    {}

    void PhaseLossMonitor::Enable()
    {
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void PhaseLossMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void PhaseLossMonitor::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        const auto& drive = activeDrive();

        if (!std::holds_alternative<state_machine::Enabled>(drive.CurrentState()))
        {
            missingFor = infra::Duration{};
            reported = false;
            return;
        }

        if (reported)
            return;

        if (!CurrentMissing(drive.ObserveMotion()))
        {
            missingFor = infra::Duration{};
            return;
        }

        missingFor += evaluationPeriod;
        if (missingFor < window)
            return;

        reported = true;
        onFault(state_machine::FaultCode::phaseMissing);
    }

    // Only judged on a slow rotor: at speed the back-EMF can legitimately leave a large demand unmet.
    bool PhaseLossMonitor::CurrentMissing(const foc::MotionObservation& observation) const
    {
        const auto demanded = std::abs(observation.demandedTorqueCurrent.Value());

        return demanded >= minimumDemandFraction * maxCurrent.Value() &&
               std::abs(observation.measuredTorqueCurrent.Value()) < missingCurrentFraction * demanded &&
               std::abs(observation.measuredSpeed.Value()) < slowSpeedRadiansPerSecond;
    }
}
