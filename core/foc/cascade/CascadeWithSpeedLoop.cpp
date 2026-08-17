#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    CascadeWithSpeedLoop::CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : lowPriorityInterrupt(lowPriorityInterrupt)
        , maxCurrent{ maxCurrent }
        , outerLoopFrequency{ lowPriorityFrequency }
        , speedDt{ 1.0f / static_cast<float>(lowPriorityFrequency.Value()) }
        , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
    {
        really_assert(maxCurrent.Value() > 0);
        really_assert(lowPriorityFrequency.Value() > 0);
        really_assert(lowPriorityFrequency.Value() <= baseFrequency.Value());
        really_assert(baseFrequency.Value() % lowPriorityFrequency.Value() == 0);
    }

    void CascadeWithSpeedLoop::ConfigureImpl(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        vdcInvScale = detail::invSqrt3 * parameters.busVoltage.Value();
        currentLoop.Configure(parameters);
    }

    void CascadeWithSpeedLoop::ConfigureMechanicsImpl(const MechanicalModelParameters& parameters)
    {
        auto withLimits = parameters;
        withLimits.maxCurrent = maxCurrent;
        withLimits.samplingFrequency = outerLoopFrequency;
        speedLoop.Configure(withLimits);
    }

    void CascadeWithSpeedLoop::SetCurrentTuningsImpl(const CurrentLoopTunings& tunings)
    {
        currentLoop.SetTunings(tunings);
    }

    void CascadeWithSpeedLoop::SetSpeedTuningsImpl(const SpeedLoopTunings& tunings)
    {
        speedLoop.SetTunings(tunings);
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::EnableSpeedLoop()
    {
        currentLoop.Reset();
        speedLoop.Reset();

        currentMechanicalAngle = 0.0f;
        previousSpeedPosition = 0.0f;
        previousSpeedPositionValid = false;
        lastSpeedLoopOutput = 0.0f;
        lastElectricalSpeed = 0.0f;
        triggerCounter = 0;
        enabled = true;
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::DisableSpeedLoop()
    {
        enabled = false;
    }

    SelectResult CascadeWithSpeedLoop::SelectCurrentAlgorithmImpl(CurrentAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return currentLoop.Select(algorithm);
    }

    SelectResult CascadeWithSpeedLoop::SelectSpeedAlgorithmImpl(SpeedAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return speedLoop.Select(algorithm);
    }

    CurrentAlgorithm CascadeWithSpeedLoop::ActiveCurrentAlgorithmImpl() const
    {
        return currentLoop.Active();
    }

    SpeedAlgorithm CascadeWithSpeedLoop::ActiveSpeedAlgorithmImpl() const
    {
        return speedLoop.Active();
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles CascadeWithSpeedLoop::CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;
        currentMechanicalAngle = mechanicalAngle;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ 0.0f, lastSpeedLoopOutput }, lastElectricalSpeed });

        auto output = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            estimators.Publish(EstimatorSnapshot{ currentPhases, electricalAngle, idAndIq.d, idAndIq.q, voltage.d });
            lowPriorityInterrupt.Trigger();
        }

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }

    void CascadeWithSpeedLoop::SetSpeedReference(RadiansPerSecond reference)
    {
        speedReference = reference;
    }

    OPTIMIZE_FOR_SPEED
    float CascadeWithSpeedLoop::MeasureMechanicalSpeed()
    {
        if (!previousSpeedPositionValid)
        {
            previousSpeedPosition = currentMechanicalAngle;
            previousSpeedPositionValid = true;
            lastElectricalSpeed = 0.0f;
            return 0.0f;
        }

        auto mechanicalSpeed = detail::PositionWithWrapAround(currentMechanicalAngle - previousSpeedPosition) / speedDt;
        previousSpeedPosition = currentMechanicalAngle;
        lastElectricalSpeed = mechanicalSpeed * polePairs;
        return mechanicalSpeed;
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::RunSpeedLoop(float mechanicalSpeed)
    {
        lastSpeedLoopOutput = speedLoop.Compute(SpeedControlContext{ RadiansPerSecond{ mechanicalSpeed }, speedReference }).Value();
    }

    OPTIMIZE_FOR_SPEED
    void CascadeWithSpeedLoop::SetDirectCurrentReference(float current)
    {
        lastSpeedLoopOutput = current;
    }

    float CascadeWithSpeedLoop::CurrentMechanicalAngle() const
    {
        return currentMechanicalAngle;
    }

    float CascadeWithSpeedLoop::PolePairs() const
    {
        return polePairs;
    }

    LowPriorityInterrupt& CascadeWithSpeedLoop::GetLowPriorityInterrupt()
    {
        return lowPriorityInterrupt;
    }

    void CascadeWithSpeedLoop::SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator)
    {
        estimators.mechanical = &estimator;
    }

    void CascadeWithSpeedLoop::SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator)
    {
        estimators.electrical = &estimator;
    }

    void CascadeWithSpeedLoop::UpdateOnlineMechanicalEstimator(float mechanicalSpeed)
    {
        if (estimators.mechanical == nullptr)
            return;

        const auto& snapshot = estimators.Ready();
        estimators.mechanical->Update(
            snapshot.phaseCurrents,
            RadiansPerSecond{ mechanicalSpeed },
            Radians{ snapshot.electricalAngle });
    }

    void CascadeWithSpeedLoop::UpdateOnlineElectricalEstimator(float electricalSpeed)
    {
        if (estimators.electrical == nullptr)
            return;

        const auto& snapshot = estimators.Ready();
        const float physicalVd = snapshot.normalizedVd * vdcInvScale;
        estimators.electrical->Update(
            Volts{ physicalVd },
            Ampere{ snapshot.measuredId },
            Ampere{ snapshot.measuredIq },
            RadiansPerSecond{ electricalSpeed });
    }
}
