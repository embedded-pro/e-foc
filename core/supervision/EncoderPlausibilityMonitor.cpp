#include "core/supervision/EncoderPlausibilityMonitor.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <cmath>
#include <variant>

namespace supervision
{
    EncoderPlausibilityMonitor::EncoderPlausibilityMonitor(drivers::Encoder& encoder, foc::Ampere maxCurrent, const infra::Function<void(state_machine::FaultCode)>& onFault)
        : encoder{ encoder }
        , maxCurrent{ maxCurrent }
        , onFault{ onFault }
    {}

    void EncoderPlausibilityMonitor::Enable(const infra::Function<Config()>& settings)
    {
        this->settings = settings;
        evaluationTimer.Start(evaluationPeriod, [this]()
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
            Disarm();
            return;
        }

        config = settings();
        const auto reading = encoder.Read().Value();
        const auto push = PushDirection(drive.ObserveMotion());
        NoteTravel(reading, push);

        if (reported || push == 0.0f)
            Restart();
        else if (StillForWholeWindow(reading))
            Report();
    }

    void EncoderPlausibilityMonitor::Disarm()
    {
        Restart();
        reported = false;
        travelAnchor = std::nullopt;
        overpowered = false;
    }

    float EncoderPlausibilityMonitor::PushDirection(const foc::MotionObservation& observation) const
    {
        if (std::abs(observation.measuredTorqueCurrent.Value()) < config.minimumCurrentFraction * maxCurrent.Value())
            return 0.0f;

        if (std::abs(observation.positionError.Value()) >= config.minimumPositionError.Value())
            return std::copysign(1.0f, observation.positionError.Value());

        if (std::abs(observation.demandedSpeed.Value()) >= config.minimumDemandedSpeed.Value())
            return std::copysign(1.0f, observation.demandedSpeed.Value());

        return 0.0f;
    }

    void EncoderPlausibilityMonitor::NoteTravel(float reading, float push)
    {
        if (!travelAnchor.has_value())
        {
            travelAnchor = reading;
            return;
        }

        const auto travel = foc::detail::PositionWithWrapAround(reading - *travelAnchor);
        if (std::abs(travel) < config.maximumStillExcursion.Value())
            return;

        travelAnchor = reading;
        overpowered = travel * push < 0.0f;
    }

    bool EncoderPlausibilityMonitor::StillForWholeWindow(float reading)
    {
        if (!tracking || std::abs(foc::detail::PositionWithWrapAround(reading - anchor)) >= config.maximumStillExcursion.Value())
        {
            anchor = reading;
            stillFor = infra::Duration{};
            tracking = true;
            return false;
        }

        stillFor += evaluationPeriod;
        return stillFor >= config.window;
    }

    void EncoderPlausibilityMonitor::Report()
    {
        Restart();

        // A stall the operator chose not to report keeps the watch going, so a later encoder loss is still raised
        if (overpowered && !config.stallDetection)
            return;

        reported = true;
        onFault(overpowered ? state_machine::FaultCode::stall : state_machine::FaultCode::encoderLoss);
    }

    void EncoderPlausibilityMonitor::Restart()
    {
        tracking = false;
        stillFor = infra::Duration{};
    }

    EncoderPlausibilityMonitor::Config ConfigFrom(const services::ConfigData& data)
    {
        const auto& stored = data.supervision;

        return EncoderPlausibilityMonitor::Config{
            .window = std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>{ stored.window }),
            .minimumCurrentFraction = stored.currentFraction,
            .minimumDemandedSpeed = foc::RadiansPerSecond{ stored.demandedSpeed },
            .minimumPositionError = foc::Radians{ stored.positionError },
            .maximumStillExcursion = foc::Radians{ stored.stillExcursion },
            .stallDetection = data.stallDetection != 0,
        };
    }
}
