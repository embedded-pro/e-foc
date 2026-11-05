#include "core/supervision/TargetReachedMonitor.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    void TargetReachedMonitor::Enable(const infra::Function<Settings()>& settings)
    {
        this->settings = settings;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void TargetReachedMonitor::Attach(const PositionDrive& positionDrive)
    {
        this->positionDrive = positionDrive;
    }

    bool TargetReachedMonitor::IsReached() const
    {
        return reached && WithinWindow();
    }

    void TargetReachedMonitor::Evaluate()
    {
        if (!WithinWindow())
        {
            withinSince.reset();
            reached = false;
            return;
        }

        const auto now = infra::Now();
        if (!withinSince.has_value())
            withinSince = now;

        reached = now - *withinSince >= settings().time;
    }

    bool TargetReachedMonitor::WithinWindow() const
    {
        if (positionDrive == nullptr || settings == nullptr)
            return false;

        const auto* drive = positionDrive();
        if (drive == nullptr || !std::holds_alternative<state_machine::Enabled>(drive->CurrentState()))
            return false;

        const auto window = settings().window.Value();
        return window > 0.0f && std::abs(drive->ObserveMotion().positionError.Value()) <= window;
    }

    TargetReachedMonitor::Settings TargetReachedSettingsFrom(const services::ConfigData& config)
    {
        const auto time = std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(config.limits.positionWindowTime));
        return { foc::Radians{ config.limits.positionWindow }, time };
    }
}
