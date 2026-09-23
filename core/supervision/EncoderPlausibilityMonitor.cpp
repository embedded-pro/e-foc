#include "core/supervision/EncoderPlausibilityMonitor.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    EncoderPlausibilityMonitor::EncoderPlausibilityMonitor(drivers::Encoder& encoder, foc::Ampere maxCurrent, const infra::Function<void()>& onEncoderLoss)
        : encoder{ encoder }
        , maxCurrent{ maxCurrent }
        , onEncoderLoss{ onEncoderLoss }
    {}

    void EncoderPlausibilityMonitor::Enable(const Config& config)
    {
        this->config = config;
        evaluationTimer.Start(config.evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void EncoderPlausibilityMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void EncoderPlausibilityMonitor::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        const auto& drive = activeDrive();

        if (!std::holds_alternative<state_machine::Enabled>(drive.CurrentState()))
        {
            Restart();
            reported = false;
            return;
        }

        if (reported || !IsPushingForMotion(drive.ObserveMotion()))
        {
            Restart();
            return;
        }

        const auto reading = encoder.Read().Value();

        if (!tracking || std::abs(foc::detail::PositionWithWrapAround(reading - anchor)) >= config.maximumStillExcursion.Value())
        {
            anchor = reading;
            stillFor = infra::Duration{};
            tracking = true;
            return;
        }

        stillFor += config.evaluationPeriod;

        if (stillFor >= config.window)
        {
            reported = true;
            Restart();
            onEncoderLoss();
        }
    }

    bool EncoderPlausibilityMonitor::IsPushingForMotion(const foc::MotionObservation& observation) const
    {
        const auto pushing = std::abs(observation.measuredTorqueCurrent.Value()) >= config.minimumCurrentFraction * maxCurrent.Value();
        const auto demanding = std::abs(observation.demandedSpeed.Value()) >= config.minimumDemandedSpeed.Value() ||
                               std::abs(observation.positionError.Value()) >= config.minimumPositionError.Value();

        return pushing && demanding;
    }

    void EncoderPlausibilityMonitor::Restart()
    {
        tracking = false;
        stillFor = infra::Duration{};
    }
}
