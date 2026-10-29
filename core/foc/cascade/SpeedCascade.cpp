#include "core/foc/cascade/SpeedCascade.hpp"

namespace foc
{
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
        SetSpeedReference(point);
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
        SetPoint(lastSpeedSetPoint);
    }

    void SpeedCascade::Disable()
    {
        DisableSpeedLoop();
    }

    void SpeedCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        if (HoldCommandedCurrent())
            return;

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
}
