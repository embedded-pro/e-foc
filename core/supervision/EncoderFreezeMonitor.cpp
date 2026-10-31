#include "core/supervision/EncoderFreezeMonitor.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <variant>

namespace supervision
{
    namespace
    {
        float Seconds(infra::Duration duration)
        {
            return std::chrono::duration<float>(duration).count();
        }
    }

    EncoderFreezeMonitor::EncoderFreezeMonitor(drivers::Encoder& encoder, const infra::Function<void(state_machine::FaultCode)>& onFault)
        : encoder{ encoder }
        , onFault{ onFault }
    {}

    void EncoderFreezeMonitor::Enable(const infra::Function<Limits()>& limits)
    {
        this->limits = limits;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void EncoderFreezeMonitor::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    void EncoderFreezeMonitor::Evaluate()
    {
        if (activeDrive == nullptr)
            return;

        if (!std::holds_alternative<state_machine::Enabled>(activeDrive().CurrentState()))
        {
            Disarm();
            return;
        }

        if (reported)
            return;

        const auto now = infra::Now();
        const auto reading = encoder.Read().Value();

        if (!anchor.has_value())
        {
            Arm(reading, now);
            return;
        }

        if (now - anchorTime < evaluationPeriod)
            return;

        const auto travel = std::abs(foc::detail::PositionWithWrapAround(reading - *anchor));

        if (travel >= 0.5f * armedLimits.encoderStep.Value())
            NoteTravel(reading, travel, now);
        else
            NoteStill(now);
    }

    void EncoderFreezeMonitor::Disarm()
    {
        anchor = std::nullopt;
        suspectSince = std::nullopt;
        lastSpeed = 0.0f;
        previousSpeed = 0.0f;
        still = false;
        reported = false;
    }

    void EncoderFreezeMonitor::Arm(float reading, infra::TimePoint now)
    {
        armedLimits = limits();
        anchor = reading;
        anchorTime = now;
    }

    // The timer advances in whole periods, so a measured span of n periods lasted up to n + 1: the speed is a
    // lower bound and the span an upper one, which only ever makes a stop look more plausible.
    void EncoderFreezeMonitor::NoteTravel(float reading, float travel, infra::TimePoint now)
    {
        previousSpeed = lastSpeed;
        previousSpan = lastSpan;
        lastSpan = now - anchorTime + evaluationPeriod;
        lastSpeed = travel / Seconds(lastSpan);
        anchor = reading;
        anchorTime = now;
        still = false;
        suspectSince = std::nullopt;
    }

    void EncoderFreezeMonitor::NoteStill(infra::TimePoint now)
    {
        if (!still)
        {
            still = true;
            if (CouldNotHaveStopped())
                suspectSince = anchorTime;
        }

        if (!suspectSince.has_value() || now - *suspectSince < stillConfirmation)
            return;

        reported = true;
        onFault(state_machine::FaultCode::encoderLoss);
    }

    // Decelerating at most alpha and ending below the speed it stops from within half a count, sqrt(alpha·step),
    // a rotor averages at most alpha·span/2 more than that over the last span, and alpha·(span/2 + last span) over
    // the one before. A freeze part-way through the last span leaves it only part of the travel, so both are judged.
    bool EncoderFreezeMonitor::CouldNotHaveStopped() const
    {
        const auto alpha = armedLimits.maximumDeceleration.Value();
        const auto stopping = std::sqrt(alpha * armedLimits.encoderStep.Value());
        const auto last = Seconds(lastSpan);

        return Exceeds(lastSpeed, stopping + alpha * last * 0.5f) ||
               Exceeds(previousSpeed, stopping + alpha * (Seconds(previousSpan) * 0.5f + last));
    }

    bool EncoderFreezeMonitor::Exceeds(float speed, float plausible)
    {
        return speed >= minimumSpeed && speed > plausible;
    }

    EncoderFreezeMonitor::Limits LimitsFrom(const services::CalibrationData& calibration, uint32_t encoderResolution, foc::Ampere maxCurrent)
    {
        // Allows a load torque as large as the motor's own
        constexpr float margin = 2.0f;
        // Torque constant over inertia, in rad/s² per ampere, for a record without one: about three times either reference motor's
        constexpr float fallbackPerAmpere = 2.0e4f;

        const auto torqueConstant = 1.5f * static_cast<float>(calibration.polePairs) * calibration.fluxLinkage;
        const auto perAmpere = torqueConstant > 0.0f && calibration.inertia > 0.0f ? torqueConstant / calibration.inertia : fallbackPerAmpere;

        return EncoderFreezeMonitor::Limits{
            .maximumDeceleration = foc::RadiansPerSecondSquared{ margin * perAmpere * maxCurrent.Value() },
            .encoderStep = foc::Radians{ 2.0f * std::numbers::pi_v<float> / static_cast<float>(std::max<uint32_t>(encoderResolution, 1)) },
        };
    }
}
