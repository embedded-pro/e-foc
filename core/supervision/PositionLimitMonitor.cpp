#include "core/supervision/PositionLimitMonitor.hpp"
#include <numbers>
#include <variant>

namespace supervision
{
    namespace
    {
        constexpr float marginCounts{ 4.0f };
    }

    PositionLimitMonitor::PositionLimitMonitor(const infra::Function<void(state_machine::FaultCode)>& onFault)
        : onFault{ onFault }
    {}

    void PositionLimitMonitor::Enable(const infra::Function<Settings()>& settings)
    {
        this->settings = settings;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void PositionLimitMonitor::Attach(const PositionDrive& positionDrive, const infra::Function<foc::Radians()>& measuredPosition)
    {
        this->positionDrive = positionDrive;
        this->measuredPosition = measuredPosition;
    }

    void PositionLimitMonitor::Evaluate()
    {
        const auto* drive = positionDrive != nullptr ? positionDrive() : nullptr;
        if (drive == nullptr || !state_machine::IsOperating(drive->CurrentState()))
        {
            reported = false;
            return;
        }

        if (reported || !OutsideLimits())
            return;

        reported = true;
        onFault(state_machine::FaultCode::positionLimit);
    }

    bool PositionLimitMonitor::OutsideLimits() const
    {
        const auto current = settings();
        if (!current.limits.IsOn())
            return false;

        const auto position = measuredPosition().Value();
        return position < current.limits.minimum.Value() - current.margin.Value() || position > current.limits.maximum.Value() + current.margin.Value();
    }

    PositionLimitMonitor::Settings PositionLimitSettingsFrom(const services::ConfigData& config)
    {
        const auto countAngle = 2.0f * std::numbers::pi_v<float> / static_cast<float>(config.encoderResolution);
        return { foc::TravelLimits{ foc::Radians{ config.limits.positionMinimum }, foc::Radians{ config.limits.positionMaximum } }, foc::Radians{ marginCounts * countAngle } };
    }
}
