#include "core/services/flux_linkage_ident/FluxLinkageIdentificationImpl.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <algorithm>
#include <cmath>

namespace services
{
    namespace
    {
        constexpr infra::Duration tickPeriod{ std::chrono::milliseconds{ 1 } };
        constexpr uint32_t minimumSamples{ 10 };
        constexpr float maximumSpeedSpanRatio{ 0.5f };
        constexpr float slowingMargin{ 1.5f };
        constexpr float recordingCeilingFraction{ 0.5f };
        constexpr float releaseLeadTicks{ 2.0f };

        bool IsUsableConfig(const FluxLinkageIdentification::Config& config)
        {
            return foc::IsFinitePositive(config.spinCurrent.Value()) &&
                   foc::IsFinitePositive(config.lowestSpeed.Value()) &&
                   config.lowestSpeed.Value() < config.releaseSpeed.Value() &&
                   config.releaseSpeed.Value() < config.maxSpeed.Value() &&
                   config.stoppedSpeed.Value() >= 0.0f &&
                   config.requiredSamples >= minimumSamples &&
                   config.maxCoasts != 0 &&
                   foc::IsFinitePositive(config.maxRelativeError) &&
                   foc::IsFinitePositive(config.maxInterceptFraction) &&
                   config.pointPeriod >= tickPeriod &&
                   config.coastLimit > infra::Duration::zero() &&
                   config.brakeLimit > infra::Duration::zero() &&
                   config.timeout > infra::Duration::zero();
        }
    }

    FluxLinkageIdentificationImpl::FluxLinkageIdentificationImpl(foc::CurrentCommandable& controller, foc::Controllable& drive, const foc::FocBase& motion, drivers::ThreePhaseInverter& inverter)
        : controller(controller)
        , drive(drive)
        , motion(motion)
        , supportedCurrent(inverter.MaxCurrentSupported())
    {}

    void FluxLinkageIdentificationImpl::Identify(std::size_t polePairs, foc::Ohm resistance, const Config& config, const infra::Function<void(const Result& result)>& onDone)
    {
        if (IsRunning() || polePairs == 0 || !foc::IsFinitePositive(resistance.Value()) || !IsUsableConfig(config))
        {
            onDone(Result{});
            return;
        }

        this->config = config;
        this->onDone = onDone;
        this->polePairs = static_cast<float>(polePairs);
        this->resistance = resistance.Value();
        spinCurrent = std::min(config.spinCurrent.Value(), supportedCurrent.Value() * spinCurrentFraction);
        recordingCeiling = config.releaseSpeed.Value() + recordingCeilingFraction * (config.maxSpeed.Value() - config.releaseSpeed.Value());
        fit = BackEmfFit{};
        coasts = 0;
        phaseTicks = 0;
        phase = Phase::accelerating;

        drive.Start();
        controller.EnableCurrentCommand();
        controller.CommandCurrent(foc::Ampere{ spinCurrent });

        tickTimer.Start(tickPeriod, [this]()
            {
                Tick();
            });

        timeoutTimer.Start(config.timeout, [this]()
            {
                StartBraking();
            });
    }

    bool FluxLinkageIdentificationImpl::IsRunning() const
    {
        return phase != Phase::idle;
    }

    void FluxLinkageIdentificationImpl::Abort()
    {
        if (!IsRunning())
            return;

        StopMotion();
        phase = Phase::idle;
        onDone = nullptr;
    }

    void FluxLinkageIdentificationImpl::Tick()
    {
        const auto observation = motion.ObserveMotion();
        const auto speed = observation.measuredSpeed.Value();

        if (std::abs(speed) > config.maxSpeed.Value())
        {
            Result result{};
            result.outcome = Outcome::overspeed;
            Finish(result);
            return;
        }

        switch (phase)
        {
            case Phase::accelerating:
                Spin(speed);
                break;
            case Phase::settling:
                if (++phaseTicks >= TicksIn(config.settleTime))
                    StartRecording();
                break;
            case Phase::recording:
                Record(speed, observation);
                break;
            case Phase::slowing:
                Slow(speed);
                break;
            case Phase::braking:
                Brake(speed);
                break;
            default:
                break;
        }
    }

    void FluxLinkageIdentificationImpl::Accelerate()
    {
        phase = Phase::accelerating;
        phaseTicks = 0;
        previousSpeed = config.maxSpeed.Value();
        controller.CommandCurrent(foc::Ampere{ spinCurrent });
    }

    void FluxLinkageIdentificationImpl::Spin(float speed)
    {
        const auto magnitude = std::abs(speed);
        const auto gainPerTick = std::max(magnitude - previousSpeed, 0.0f);
        previousSpeed = magnitude;

        if (magnitude + releaseLeadTicks * gainPerTick >= config.releaseSpeed.Value())
            Settle();
    }

    void FluxLinkageIdentificationImpl::Settle()
    {
        phase = Phase::settling;
        phaseTicks = 0;
        controller.CommandCurrent(foc::Ampere{ 0.0f });
    }

    void FluxLinkageIdentificationImpl::StartRecording()
    {
        phase = Phase::recording;
        phaseTicks = 0;
        pointSpeed = 0.0f;
        pointVoltage = 0.0f;
        pointTicks = 0;
        previousSpeed = config.maxSpeed.Value();
    }

    void FluxLinkageIdentificationImpl::Record(float speed, const foc::MotionObservation& observation)
    {
        if (std::abs(speed) < config.lowestSpeed.Value() || phaseTicks >= TicksIn(config.coastLimit))
        {
            EndCoast(speed);
            return;
        }

        ++phaseTicks;
        pointSpeed += speed;
        pointVoltage += controller.QuadratureVoltage().Value() - resistance * observation.measuredTorqueCurrent.Value();

        if (++pointTicks == TicksIn(config.pointPeriod))
            AddPoint();
    }

    void FluxLinkageIdentificationImpl::AddPoint()
    {
        const auto scale = 1.0f / static_cast<float>(pointTicks);
        const auto meanSpeed = std::abs(pointSpeed * scale);
        fit.Add(polePairs * pointSpeed * scale, pointVoltage * scale);
        pointSpeed = 0.0f;
        pointVoltage = 0.0f;
        pointTicks = 0;

        const bool speedingUp = meanSpeed > recordingCeiling && meanSpeed > previousSpeed;
        previousSpeed = meanSpeed;

        if (speedingUp)
            EndCoast(meanSpeed);
    }

    void FluxLinkageIdentificationImpl::EndCoast(float speed)
    {
        ++coasts;

        if (coasts >= config.maxCoasts || (fit.Count() >= config.requiredSamples && SpansSpeeds()))
            StartBraking();
        else if (std::abs(speed) > recordingCeiling || !SpansSpeeds())
            phase = Phase::slowing;
        else
            Accelerate();
    }

    void FluxLinkageIdentificationImpl::Slow(float speed)
    {
        if (std::abs(speed) <= slowingMargin * config.lowestSpeed.Value())
            Settle();
        else
            controller.CommandCurrent(foc::Ampere{ speed > 0.0f ? -spinCurrent : spinCurrent });
    }

    bool FluxLinkageIdentificationImpl::SpansSpeeds() const
    {
        return fit.Count() != 0 && fit.LowestSpeed() <= maximumSpeedSpanRatio * fit.HighestSpeed();
    }

    void FluxLinkageIdentificationImpl::Brake(float speed)
    {
        if (std::abs(speed) <= config.stoppedSpeed.Value() || ++phaseTicks >= TicksIn(config.brakeLimit))
        {
            Finish(Evaluate());
            return;
        }

        controller.CommandCurrent(foc::Ampere{ speed > 0.0f ? -spinCurrent : spinCurrent });
    }

    void FluxLinkageIdentificationImpl::StartBraking()
    {
        timeoutTimer.Cancel();
        phase = Phase::braking;
        phaseTicks = 0;
    }

    void FluxLinkageIdentificationImpl::Finish(const Result& result)
    {
        StopMotion();
        phase = Phase::idle;
        onDone(result);
    }

    void FluxLinkageIdentificationImpl::StopMotion()
    {
        tickTimer.Cancel();
        timeoutTimer.Cancel();
        controller.CommandCurrent(foc::Ampere{ 0.0f });
        controller.DisableCurrentCommand();
        drive.Stop();
    }

    FluxLinkageIdentification::Result FluxLinkageIdentificationImpl::Evaluate() const
    {
        Result result{};
        result.samples = fit.Count();
        result.relativeError = fit.RelativeSlopeError();
        result.intercept = foc::Volts{ fit.Intercept() };
        result.fluxLinkage = foc::Weber{ fit.Slope() };

        const bool enoughSamples = fit.Count() >= minimumSamples;
        const bool spansSpeeds = SpansSpeeds();
        const bool precise = result.relativeError <= config.maxRelativeError;
        const bool smallOffset = std::abs(result.intercept.Value()) <= config.maxInterceptFraction * fit.LargestVoltage();
        const bool plausible = foc::IsWithinInclusive(result.fluxLinkage.Value(), foc::CommandLimits::minFluxLinkage, foc::CommandLimits::maxFluxLinkage);

        result.outcome = enoughSamples && spansSpeeds && precise && smallOffset && plausible ? Outcome::identified : Outcome::notIdentified;
        return result;
    }

    uint32_t FluxLinkageIdentificationImpl::TicksIn(infra::Duration duration) const
    {
        return static_cast<uint32_t>(duration / tickPeriod);
    }
}
