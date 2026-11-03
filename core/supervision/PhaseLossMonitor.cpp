#include "core/supervision/PhaseLossMonitor.hpp"
#include <algorithm>
#include <cmath>
#include <variant>

namespace supervision
{
    PhaseLossMonitor::PhaseLossMonitor(foc::Ampere maxCurrent, const infra::Function<void(state_machine::FaultCode)>& onFault)
        : maxCurrent{ maxCurrent }
        , onFault{ onFault }
    {}

    void PhaseLossMonitor::Enable(const infra::Function<foc::RadiansPerSecond()>& currentLoopBandwidth)
    {
        this->currentLoopBandwidth = currentLoopBandwidth;
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
            missingSince.reset();
            reported = false;
            return;
        }

        if (reported)
            return;

        if (!CurrentMissing(drive.ObserveMotion()))
        {
            missingSince.reset();
            return;
        }

        const auto now = infra::Now();
        if (!missingSince.has_value())
            missingSince = now;

        if (now - *missingSince < Span())
            return;

        reported = true;
        onFault(state_machine::FaultCode::phaseMissing);
    }

    infra::Duration PhaseLossMonitor::Span() const
    {
        const auto bandwidth = currentLoopBandwidth != nullptr ? currentLoopBandwidth().Value() : 0.0f;

        if (bandwidth <= 0.0f)
            return window;

        const auto timeConstant = std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(1.0f / bandwidth));
        return std::max(window, timeConstant);
    }

    bool PhaseLossMonitor::CurrentMissing(const foc::MotionObservation& observation) const
    {
        const auto demanded = std::abs(observation.demandedTorqueCurrent.Value());

        return demanded >= minimumDemandFraction * maxCurrent.Value() &&
               std::abs(observation.measuredTorqueCurrent.Value()) < missingCurrentFraction * demanded &&
               std::abs(observation.measuredSpeed.Value()) < slowSpeedRadiansPerSecond;
    }
}
