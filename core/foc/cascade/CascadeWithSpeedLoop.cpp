#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include <algorithm>
#include <chrono>

namespace foc
{
    namespace
    {
        constexpr float excitationCurrentFraction{ 0.025f };
        const hal::Hertz excitationFrequency{ 50 };
        constexpr std::chrono::seconds excitationBurst{ 4 };
        constexpr std::chrono::seconds excitationCycle{ 60 };
    }

    CascadeWithSpeedLoop::CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : lowPriorityInterrupt(lowPriorityInterrupt)
        , maxCurrent{ maxCurrent }
        , outerLoopFrequency{ lowPriorityFrequency }
        , speedDifferentiator{ lowPriorityFrequency }
        , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
        , estimatorWindow{ baseFrequency, prescaler, dutyUpdateDelayPeriods }
        , directAxisExcitation{ Ampere{ maxCurrent.Value() * excitationCurrentFraction }, excitationFrequency, lowPriorityFrequency, excitationBurst, excitationCycle }
    {
        really_assert(maxCurrent.Value() > 0);
        really_assert(lowPriorityFrequency.Value() > 0);
        really_assert(lowPriorityFrequency.Value() <= baseFrequency.Value());
        really_assert(baseFrequency.Value() % lowPriorityFrequency.Value() == 0);
    }

    CascadeWithSpeedLoop::~CascadeWithSpeedLoop()
    {
        lowPriorityInterrupt.Unregister();
    }

    bool CascadeWithSpeedLoop::ConfigureImpl(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        vdcInvScale = std::numbers::inv_sqrt3_v<float> * parameters.busVoltage.Value();
        return currentLoop.Configure(parameters);
    }

    bool CascadeWithSpeedLoop::ConfigureMechanicsImpl(MechanicalModelParameters& parameters)
    {
        parameters.maxCurrent = maxCurrent;
        parameters.samplingFrequency = outerLoopFrequency;
        return speedLoop.Configure(parameters);
    }

    void CascadeWithSpeedLoop::SetCurrentTuningsImpl(const CurrentLoopTunings& tunings)
    {
        currentLoop.SetTunings(tunings);
    }

    SelectResult CascadeWithSpeedLoop::SetSpeedTuningsImpl(const SpeedLoopTunings& tunings)
    {
        return speedLoop.TrySetTunings(tunings);
    }

    void CascadeWithSpeedLoop::EnableSpeedLoop()
    {
        currentLoop.Reset();
        speedLoop.Reset();

        speedDifferentiator.Restart();
        estimatorWindow.Restart();
        directAxisExcitation.Restart();
        lastSpeedLoopOutput = 0.0f;
        directCurrentReference = 0.0f;
        lastElectricalSpeed = 0.0f;
        lastMechanicalSpeed = 0.0f;
        lastMeanIq = 0.0f;
        triggerCounter = 0;
        enabled = true;
    }

    void CascadeWithSpeedLoop::DisableSpeedLoop()
    {
        enabled = false;
        lastElectricalSpeed = 0.0f;
        lastMechanicalSpeed = 0.0f;
        lastMeanIq = 0.0f;
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

    PhasePwmDutyCycles CascadeWithSpeedLoop::CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;
        speedDifferentiator.Track(mechanicalAngle);

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        const auto coggingCurrent = coggingFeedForward.CurrentAt(electricalAngle);
        const auto quadratureReference = std::clamp(lastSpeedLoopOutput + coggingCurrent, -maxCurrent.Value(), maxCurrent.Value());
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ directCurrentReference, quadratureReference }, lastElectricalSpeed });

        const auto duties = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));

        estimatorWindow.Accumulate(idAndIq, voltage, mechanicalAngle, polePairs, coggingCurrent);

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            speedDifferentiator.CloseWindow();
            const auto window = estimatorWindow.Close();
            lastMeanIq = window.meanIq;
            estimators.Publish(window);
            lowPriorityInterrupt.Trigger();
        }

        return duties;
    }

    void CascadeWithSpeedLoop::SetSpeedReference(RadiansPerSecond reference)
    {
        speedReference = reference.Value();
    }

    float CascadeWithSpeedLoop::MeasureMechanicalSpeed()
    {
        const auto mechanicalSpeed = speedDifferentiator.Measure();
        lastMechanicalSpeed = mechanicalSpeed;
        lastElectricalSpeed = mechanicalSpeed * polePairs;
        return mechanicalSpeed;
    }

    void CascadeWithSpeedLoop::RunSpeedLoop(float mechanicalSpeed)
    {
        lastSpeedLoopOutput = speedLoop.Compute(SpeedControlContext{ RadiansPerSecond{ mechanicalSpeed }, RadiansPerSecond{ speedReference } }).Value();
    }

    void CascadeWithSpeedLoop::SetDirectCurrentReference(float current)
    {
        lastSpeedLoopOutput = current;
    }

    float CascadeWithSpeedLoop::CurrentMechanicalAngle() const
    {
        return speedDifferentiator.CurrentAngle();
    }

    MotionObservation CascadeWithSpeedLoop::ObserveSpeedLoopMotion() const
    {
        return { RadiansPerSecond{ lastMechanicalSpeed }, Ampere{ lastMeanIq }, RadiansPerSecond{ speedReference }, Radians{ 0.0f } };
    }

    LowPriorityInterrupt& CascadeWithSpeedLoop::GetLowPriorityInterrupt()
    {
        return lowPriorityInterrupt;
    }

    void CascadeWithSpeedLoop::SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator)
    {
        estimators.SetMechanical(estimator);
    }

    void CascadeWithSpeedLoop::SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator)
    {
        estimators.SetElectrical(estimator);
    }

    void CascadeWithSpeedLoop::UpdateOnlineMechanicalEstimator()
    {
        estimators.UpdateMechanical();
    }

    void CascadeWithSpeedLoop::UpdateOnlineElectricalEstimator()
    {
        if (!estimators.HasElectrical())
            return;

        estimators.UpdateElectrical(vdcInvScale, Ampere{ directCurrentReference });
        directCurrentReference = directAxisExcitation.Advance();
    }

    void CascadeWithSpeedLoop::LoadCoggingMapImpl(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        coggingFeedForward.Load(torque, torqueScale, torqueConstant);
    }

    void CascadeWithSpeedLoop::ClearCoggingMapImpl()
    {
        coggingFeedForward.Clear();
    }
}
