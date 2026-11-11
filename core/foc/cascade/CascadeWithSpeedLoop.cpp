#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "core/foc/transforms/DelayCompensation.hpp"
#include "numerical/math/FastTrigonometry.hpp"
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
        , schedule{ lowPriorityFrequency, baseFrequency.Value() / lowPriorityFrequency.Value(), 0 }
        , speedDifferentiator{ lowPriorityFrequency }
        , command{ 0.0f, maxCurrent.Value(), false, QuadratureVoltageWindow{} }
        , estimation{ EstimatorChannel{}, EstimatorWindowAccumulator{ baseFrequency, schedule.prescaler, dutyUpdateDelayPeriods }, 0.0f }
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
        scaling.polePairs = static_cast<float>(parameters.polePairs);
        scaling.vdcInvScale = std::numbers::inv_sqrt3_v<float> * parameters.busVoltage.Value();
        busRatio.Configure(parameters.busVoltage);
        scaling.dutyDelay = DutyDelay(parameters.dutyUpdateDelayPeriods, static_cast<float>(parameters.samplingFrequency.Value()));
        zeroVectorSlope.Configure(parameters);
        return currentLoop.Configure(parameters);
    }

    bool CascadeWithSpeedLoop::ConfigureMechanicsImpl(MechanicalModelParameters& parameters)
    {
        parameters.maxCurrent = maxCurrent;
        parameters.samplingFrequency = schedule.frequency;
        accelerationCurrentGain = parameters.torqueConstant.Value() > 0.0f ? parameters.inertia.Value() / parameters.torqueConstant.Value() : 0.0f;
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
        command.active = false;
        command.voltageWindow.Close();
        currentLoop.Reset();
        speedLoop.Reset();

        speedDifferentiator.Restart();
        estimation.window.Restart();
        directAxisExcitation.Restart();
        zeroVectorSlope.Restart();
        outer.speedLoopOutput = 0.0f;
        outer.feedForwardCurrent = 0.0f;
        outer.directCurrentReference = 0.0f;
        outer.electricalSpeed = 0.0f;
        outer.mechanicalSpeed = 0.0f;
        estimation.meanIq = 0.0f;
        schedule.triggerCounter = 0;
        if (currentStream != nullptr)
            currentStream->Restart();
        enabled = true;
    }

    void CascadeWithSpeedLoop::DisableSpeedLoop()
    {
        enabled = false;
        command.active = false;
        command.voltageWindow.Close();
        outer.electricalSpeed = 0.0f;
        outer.mechanicalSpeed = 0.0f;
        estimation.meanIq = 0.0f;
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
        auto electricalAngle = mechanicalAngle * scaling.polePairs;
        speedDifferentiator.Track(mechanicalAngle);

        auto cosTheta = math::FastCosine(electricalAngle);
        auto sinTheta = math::FastSine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        if (currentStream != nullptr)
            currentStream->Accumulate(idAndIq.d, idAndIq.q, mechanicalAngle);
        const auto coggingCurrent = feedForward.cogging.CurrentAt(electricalAngle);
        const float limit = command.quadratureLimit;
        const auto quadratureReference = std::clamp(outer.speedLoopOutput + outer.feedForwardCurrent + coggingCurrent, -limit, limit);
        const float electricalSpeed = outer.electricalSpeed;
        if (slopeSink != nullptr)
            slopeSink->ZeroVectorSlopes(zeroVectorSlope.Next(idAndIq, electricalSpeed, cosTheta, sinTheta));
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ outer.directCurrentReference, quadratureReference }, electricalSpeed, busRatio.Ratio() });

        const auto applied = AdvanceRotation(cosTheta, sinTheta, electricalSpeed, scaling.dutyDelay);
        const auto duties = spaceVectorModulator.Generate(park.Inverse(voltage, applied.cosine, applied.sine), busRatio.Inverse());
        command.voltageWindow.Accumulate(voltage.q);

        estimation.window.Accumulate(idAndIq, voltage, mechanicalAngle, scaling.polePairs, coggingCurrent);

        ++schedule.triggerCounter;
        if (schedule.triggerCounter >= schedule.prescaler)
        {
            schedule.triggerCounter = 0;
            speedDifferentiator.CloseWindow();
            const auto window = estimation.window.Close();
            estimation.meanIq = window.meanIq;
            estimation.channel.Publish(window);
            lowPriorityInterrupt.Trigger();
        }

        return duties;
    }

    void CascadeWithSpeedLoop::TrackBusVoltageImpl(Volts measured)
    {
        busRatio.Track(measured);
    }

    void CascadeWithSpeedLoop::LimitCurrentImpl(Ampere limit)
    {
        command.quadratureLimit = std::clamp(limit.Value(), 0.0f, maxCurrent.Value());
    }

    void CascadeWithSpeedLoop::SetSpeedReference(RadiansPerSecond reference)
    {
        outer.speedReference = reference.Value();
    }

    float CascadeWithSpeedLoop::MeasureMechanicalSpeed()
    {
        const auto mechanicalSpeed = speedDifferentiator.Measure();
        outer.mechanicalSpeed = mechanicalSpeed;
        outer.electricalSpeed = mechanicalSpeed * scaling.polePairs;
        return mechanicalSpeed;
    }

    void CascadeWithSpeedLoop::RunSpeedLoop(float mechanicalSpeed, float referenceAcceleration)
    {
        outer.speedLoopOutput = speedLoop.Compute(SpeedControlContext{ RadiansPerSecond{ mechanicalSpeed }, RadiansPerSecond{ outer.speedReference } }).Value();
        outer.feedForwardCurrent = feedForward.friction.CurrentFor(outer.speedReference) + accelerationCurrentGain * referenceAcceleration;
    }

    void CascadeWithSpeedLoop::SetDirectCurrentReference(float current, float referenceAcceleration)
    {
        outer.speedLoopOutput = current;
        outer.feedForwardCurrent = accelerationCurrentGain * referenceAcceleration;
    }

    void CascadeWithSpeedLoop::EnableCurrentCommandImpl()
    {
        EnableSpeedLoop();
        command.current = 0.0f;
        command.voltageWindow.Open(schedule.prescaler);
        command.active = true;
    }

    void CascadeWithSpeedLoop::CommandCurrentImpl(Ampere quadrature)
    {
        command.current = quadrature.Value();
    }

    Volts CascadeWithSpeedLoop::QuadratureVoltageImpl() const
    {
        return Volts{ command.voltageWindow.Mean() * scaling.vdcInvScale };
    }

    bool CascadeWithSpeedLoop::HoldCommandedCurrent()
    {
        if (!command.active)
            return false;

        SetDirectCurrentReference(command.current);
        return true;
    }

    float CascadeWithSpeedLoop::CurrentMechanicalAngle() const
    {
        return speedDifferentiator.CurrentAngle();
    }

    MotionObservation CascadeWithSpeedLoop::ObserveSpeedLoopMotion() const
    {
        const float limit = command.quadratureLimit;
        const float demanded = outer.speedLoopOutput + outer.feedForwardCurrent;
        return { RadiansPerSecond{ outer.mechanicalSpeed }, Ampere{ estimation.meanIq }, RadiansPerSecond{ outer.speedReference }, Radians{ 0.0f }, Ampere{ std::clamp(demanded, -limit, limit) } };
    }

    LowPriorityInterrupt& CascadeWithSpeedLoop::GetLowPriorityInterrupt()
    {
        return lowPriorityInterrupt;
    }

    void CascadeWithSpeedLoop::SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator)
    {
        estimation.channel.SetMechanical(estimator);
    }

    void CascadeWithSpeedLoop::SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator)
    {
        estimation.channel.SetElectrical(estimator);
    }

    void CascadeWithSpeedLoop::UpdateOnlineMechanicalEstimator()
    {
        estimation.channel.UpdateMechanical();
    }

    void CascadeWithSpeedLoop::UpdateOnlineElectricalEstimator()
    {
        if (!estimation.channel.HasElectrical())
            return;

        estimation.channel.UpdateElectrical(scaling.vdcInvScale, Ampere{ outer.directCurrentReference });
        outer.directCurrentReference = directAxisExcitation.Advance();

        if (const auto estimate = estimation.channel.SettledElectricalEstimate())
            zeroVectorSlope.Track(estimate->first, estimate->second);
    }

    void CascadeWithSpeedLoop::LoadCoggingMapImpl(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        feedForward.cogging.Load(torque, torqueScale, torqueConstant);
    }

    void CascadeWithSpeedLoop::ClearCoggingMapImpl()
    {
        feedForward.cogging.Clear();
    }

    void CascadeWithSpeedLoop::LoadFrictionFeedForwardImpl(NewtonMeter coulomb, NewtonMeter torqueConstant)
    {
        feedForward.friction.Load(coulomb, torqueConstant, maxCurrent);
    }

    void CascadeWithSpeedLoop::ClearFrictionFeedForwardImpl()
    {
        feedForward.friction.Clear();
        outer.feedForwardCurrent = 0.0f;
    }

    void CascadeWithSpeedLoop::SetCurrentStream(CurrentStream& stream)
    {
        currentStream = &stream;
    }

    void CascadeWithSpeedLoop::SetZeroVectorSlopeSink(ZeroVectorSlopeSink& sink)
    {
        slopeSink = &sink;
    }
}
