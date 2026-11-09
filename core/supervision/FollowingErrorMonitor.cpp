#include "core/supervision/FollowingErrorMonitor.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    FollowingErrorMonitor::FollowingErrorMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault)
        : onFault{ onFault }
    {}

    void FollowingErrorMonitor::Enable(const infra::Function<Settings()>& settings)
    {
        this->settings = settings;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void FollowingErrorMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void FollowingErrorMonitor::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        const auto& drive = activeDrive();

        if (!std::holds_alternative<state_machine::Enabled>(drive.CurrentState()))
        {
            beyond.Reset();
            return;
        }

        const auto current = settings();
        const auto counts = !beyond.Reported() && current.window.Value() > 0.0f && std::abs(drive.ObserveMotion().positionError.Value()) > current.window.Value();

        if (beyond.ReportOnce(beyond.Held(counts, infra::Now(), current.time)))
            onFault(state_machine::FaultCode::followingError);
    }

    FollowingErrorMonitor::Settings FollowingErrorSettingsFrom(const services::ConfigData& config)
    {
        const auto time = std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(config.limits.followingErrorTime));
        return { foc::Radians{ config.limits.followingErrorWindow }, time };
    }
}
