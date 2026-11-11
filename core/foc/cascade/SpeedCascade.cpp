#include "core/foc/cascade/SpeedCascade.hpp"
#include <algorithm>

namespace foc
{
    namespace
    {
        bool MovesAwayFromZero(float reference, float target)
        {
            return (target > reference && reference >= 0.0f) || (target < reference && reference <= 0.0f);
        }

        float Towards(float reference, float stop, float step)
        {
            return reference > stop ? std::max(stop, reference - step) : std::min(stop, reference + step);
        }

        float RampTowards(float reference, float target, float accelerationStep, float decelerationStep)
        {
            if (MovesAwayFromZero(reference, target))
                return accelerationStep > 0.0f ? Towards(reference, target, accelerationStep) : target;

            const auto stop = target * reference < 0.0f ? 0.0f : target;
            return decelerationStep > 0.0f ? Towards(reference, stop, decelerationStep) : stop;
        }
    }

    SpeedCascade::SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, CurrentStream& currentStream, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : SpeedLoopDrive(maxCurrent, baseFrequency, lowPriorityInterrupt, currentStream, lowPriorityFrequency, dutyUpdateDelayPeriods)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    bool SpeedCascade::ConfigureMechanics(const MechanicalModelParameters& parameters)
    {
        auto withLimits = parameters;
        return ConfigureMechanicsImpl(withLimits);
    }

    void SpeedCascade::SetPoint(RadiansPerSecond point)
    {
        lastSpeedSetPoint = point;
        target = point.Value();

        if (!IsRamped())
            ApplyReference(point.Value());
    }

    void SpeedCascade::RampSpeed(RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration)
    {
        const auto frequency = static_cast<float>(outerLoopFrequency.Value());
        accelerationStep = std::max(0.0f, acceleration.Value()) / frequency;
        decelerationStep = std::max(0.0f, deceleration.Value()) / frequency;

        if (!IsRamped())
            ApplyReference(target);
    }

    bool SpeedCascade::IsRamped() const
    {
        return !speedCommandActive && (accelerationStep > 0.0f || decelerationStep > 0.0f);
    }

    void SpeedCascade::ApplyReference(float value)
    {
        reference = value;
        SetSpeedReference(RadiansPerSecond{ value });
    }

    void SpeedCascade::RampReference()
    {
        if (IsRamped())
            ApplyReference(RampTowards(reference, target, accelerationStep, decelerationStep));
    }

    void SpeedCascade::Enable()
    {
        EnableSpeedLoop();
        if (IsRamped())
            ApplyReference(0.0f);
        SetPoint(lastSpeedSetPoint);
    }

    void SpeedCascade::Disable()
    {
        speedCommandActive = false;
        DisableSpeedLoop();
    }

    void SpeedCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        if (HoldCommandedCurrent())
            return;

        RampReference();
        RunSpeedLoop(mechanicalSpeed);

        UpdateOnlineMechanicalEstimator();
        UpdateOnlineElectricalEstimator();
    }

    void SpeedCascade::EnableCurrentCommand()
    {
        EnableCurrentCommandImpl();
    }

    void SpeedCascade::DisableCurrentCommand()
    {
        Disable();
    }

    MotionObservation SpeedCascade::ObserveMotion() const
    {
        return ObserveSpeedLoopMotion();
    }

    hal::Hertz SpeedCascade::OuterLoopFrequency() const
    {
        return outerLoopFrequency;
    }

    void SpeedCascade::EnableSpeedCommand()
    {
        speedCommandActive = true;
        Enable();
    }

    void SpeedCascade::DisableSpeedCommand()
    {
        Disable();
    }

    void SpeedCascade::CommandSpeed(RadiansPerSecond speed)
    {
        SetPoint(speed);
    }

    hal::Hertz SpeedCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

    PhasePwmDutyCycles SpeedCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }

}
