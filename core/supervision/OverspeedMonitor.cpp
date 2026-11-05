#include "core/supervision/OverspeedMonitor.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    OverspeedMonitor::OverspeedMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault)
        : onFault{ onFault }
    {}

    void OverspeedMonitor::Enable(const infra::Function<foc::RadiansPerSecond()>& speedLimit)
    {
        this->speedLimit = speedLimit;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void OverspeedMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void OverspeedMonitor::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        const auto& drive = activeDrive();

        if (!std::holds_alternative<state_machine::Enabled>(drive.CurrentState()))
        {
            evaluationsBeyond = 0;
            reported = false;
            return;
        }

        if (reported)
            return;

        if (!BeyondLimit(drive))
        {
            evaluationsBeyond = 0;
            return;
        }

        if (++evaluationsBeyond < evaluationsToFault)
            return;

        reported = true;
        onFault(state_machine::FaultCode::overspeed);
    }

    bool OverspeedMonitor::BeyondLimit(const state_machine::FocStateMachineBase& drive) const
    {
        return std::abs(drive.ObserveMotion().measuredSpeed.Value()) > faultMargin * speedLimit().Value();
    }
}
