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

    SpeedCascade::SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency, dutyUpdateDelayPeriods)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    bool SpeedCascade::Configure(const MotorModelParameters& parameters)
    {
        return ConfigureImpl(parameters);
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

        if (!IsRamped() && !IsQuickStopping())
            ApplyReference(point.Value());
    }

    void SpeedCascade::QuickStop(RadiansPerSecondSquared deceleration)
    {
        BeginQuickStop(deceleration);
    }

    void SpeedCascade::RampSpeed(RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration)
    {
        const auto frequency = static_cast<float>(outerLoopFrequency.Value());
        accelerationStep = std::max(0.0f, acceleration.Value()) / frequency;
        decelerationStep = std::max(0.0f, deceleration.Value()) / frequency;

        if (!IsRamped() && !IsQuickStopping())
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

    void SpeedCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        SetCurrentTuningsImpl(tunings);
    }

    SelectResult SpeedCascade::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        return SetSpeedTuningsImpl(tunings);
    }

    SelectResult SpeedCascade::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        return SelectCurrentAlgorithmImpl(algorithm);
    }

    CurrentAlgorithm SpeedCascade::ActiveCurrentAlgorithm() const
    {
        return ActiveCurrentAlgorithmImpl();
    }

    SelectResult SpeedCascade::SelectSpeedAlgorithm(SpeedAlgorithm algorithm)
    {
        return SelectSpeedAlgorithmImpl(algorithm);
    }

    SpeedAlgorithm SpeedCascade::ActiveSpeedAlgorithm() const
    {
        return ActiveSpeedAlgorithmImpl();
    }

    void SpeedCascade::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    void SpeedCascade::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
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

        if (HoldCommandedCurrent() || RunQuickStop(mechanicalSpeed))
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

    void SpeedCascade::CommandCurrent(Ampere quadrature)
    {
        CommandCurrentImpl(quadrature);
    }

    Volts SpeedCascade::QuadratureVoltage() const
    {
        return QuadratureVoltageImpl();
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

    void SpeedCascade::LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        LoadCoggingMapImpl(torque, torqueScale, torqueConstant);
    }

    void SpeedCascade::ClearCoggingMap()
    {
        ClearCoggingMapImpl();
    }

    void SpeedCascade::TrackBusVoltage(Volts measured)
    {
        TrackBusVoltageImpl(measured);
    }

    void SpeedCascade::LimitCurrent(Ampere limit)
    {
        LimitCurrentImpl(limit);
    }
}
