#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/transforms/DelayCompensation.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/FastTrigonometry.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace foc
{
    namespace
    {
        constexpr float speedFilterTimeConstant = 0.001f;
        constexpr float speedFoldGain = 10.0f;

        float FoldedQuadrature(float quadrature, float electricalSpeed, float inverseElectricalSpeedLimit)
        {
            if (quadrature * electricalSpeed <= 0.0f)
                return quadrature;

            return quadrature * std::clamp(speedFoldGain * (1.0f - math::Abs(electricalSpeed) * inverseElectricalSpeedLimit), 0.0f, 1.0f);
        }
    }

    TorqueCascade::TorqueCascade(foc::Ampere maxCurrent, CurrentStream& currentStream)
        : maxCurrent{ maxCurrent }
        , currentStream{ currentStream }
    {
        really_assert(maxCurrent.Value() > 0);
        limits.current = maxCurrent.Value();
    }

    void TorqueCascade::Enable()
    {
        voltageWindow.Close();
        currentLoop.Reset();
        enabled = true;
        speedMeasurement.filter.Reset();
        speedMeasurement.previousAngleValid = false;
        lastElectricalSpeed = 0.0f;
        lastIq = 0.0f;
        currentStream.Restart();
        SetPoint(requestedSetPoint);
    }

    void TorqueCascade::Disable()
    {
        enabled = false;
        currentCommandActive = false;
        voltageWindow.Close();
        lastElectricalSpeed = 0.0f;
        lastIq = 0.0f;
    }

    SelectResult TorqueCascade::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return currentLoop.Select(algorithm);
    }

    CurrentAlgorithm TorqueCascade::ActiveCurrentAlgorithm() const
    {
        return currentLoop.Active();
    }

    bool TorqueCascade::Configure(const MotorModelParameters& parameters)
    {
        motor.polePairs = static_cast<float>(parameters.polePairs);
        UpdateSpeedLimit();
        const bool ok = currentLoop.Configure(parameters);
        zeroVectorSlope.Configure(parameters);

        const auto samplingFrequency = static_cast<float>(parameters.samplingFrequency.Value());
        speedMeasurement.scale = motor.polePairs * samplingFrequency;
        motor.dutyDelay = DutyDelay(parameters.dutyUpdateDelayPeriods, samplingFrequency);
        voltageScale = std::numbers::inv_sqrt3_v<float> * parameters.busVoltage.Value();
        busRatio.Configure(parameters.busVoltage);
        samplesPerMillisecond = std::max<uint32_t>(1, parameters.samplingFrequency.Value() / 1000);
        speedMeasurement.filter.SetAlpha(1.0f - math::Exp(-1.0f / (speedFilterTimeConstant * samplingFrequency)));
        return ok;
    }

    void TorqueCascade::SetPoint(IdAndIqPoint setPoint)
    {
        requestedSetPoint = LimitToCurrentEnvelope(setPoint);
        setPointBuffer.Publish(requestedSetPoint);
    }

    IdAndIqPoint TorqueCascade::LimitToCurrentEnvelope(IdAndIqPoint setPoint) const
    {
        const auto d = setPoint.first.Value();
        const auto q = setPoint.second.Value();
        const auto squaredMagnitude = d * d + q * q;
        const auto limit = maxCurrent.Value();

        if (squaredMagnitude <= limit * limit)
            return setPoint;

        const auto scale = limit / math::Sqrt(squaredMagnitude);

        return { Ampere{ d * scale }, Ampere{ q * scale } };
    }

    void TorqueCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        currentLoop.SetTunings(tunings);
    }

    MotionObservation TorqueCascade::ObserveMotion() const
    {
        const auto mechanicalSpeed = motor.polePairs > 0.0f ? lastElectricalSpeed / motor.polePairs : 0.0f;

        const auto demanded = LimitedQuadrature(requestedSetPoint.second.Value(), lastElectricalSpeed, 0.0f);

        return { RadiansPerSecond{ mechanicalSpeed }, Ampere{ lastIq }, RadiansPerSecond{ 0.0f }, Radians{ 0.0f }, Ampere{ demanded } };
    }

    void TorqueCascade::LimitSpeed(RadiansPerSecond limit)
    {
        limits.speed = limit;
        UpdateSpeedLimit();
    }

    void TorqueCascade::TrackBusVoltage(Volts measured)
    {
        busRatio.Track(measured);
    }

    void TorqueCascade::LimitCurrent(Ampere limit)
    {
        limits.current = std::clamp(limit.Value(), 0.0f, maxCurrent.Value());
    }

    float TorqueCascade::LimitedQuadrature(float quadrature, float electricalSpeed, float feedForward) const
    {
        const float limit = limits.current;
        return std::clamp(FoldedQuadrature(quadrature, electricalSpeed, limits.inverseElectricalSpeed) + feedForward, -limit, limit);
    }

    void TorqueCascade::LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        coggingFeedForward.Load(torque, torqueScale, torqueConstant);
    }

    void TorqueCascade::ClearCoggingMap()
    {
        coggingFeedForward.Clear();
    }

    void TorqueCascade::UpdateSpeedLimit()
    {
        const auto electricalLimit = limits.speed.Value() * motor.polePairs;
        limits.inverseElectricalSpeed = electricalLimit > 0.0f ? 1.0f / electricalLimit : 0.0f;
    }

    void TorqueCascade::SetZeroVectorSlopeSink(ZeroVectorSlopeSink& sink)
    {
        slopeSink = &sink;
    }

    void TorqueCascade::EnableCurrentCommand()
    {
        requestedSetPoint = IdAndIqPoint{ Ampere{ 0.0f }, Ampere{ 0.0f } };
        currentCommandActive = true;
        Enable();
        voltageWindow.Open(samplesPerMillisecond);
    }

    void TorqueCascade::DisableCurrentCommand()
    {
        Disable();
    }

    void TorqueCascade::CommandCurrent(Ampere quadrature)
    {
        SetPoint(IdAndIqPoint{ Ampere{ 0.0f }, quadrature });
    }

    Volts TorqueCascade::QuadratureVoltage() const
    {
        return Volts{ voltageWindow.Mean() * voltageScale };
    }

    PhasePwmDutyCycles TorqueCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * motor.polePairs;

        auto cosTheta = math::FastCosine(electricalAngle);
        auto sinTheta = math::FastSine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        const auto electricalSpeed = MeasureElectricalSpeed(mechanicalAngle);
        if (slopeSink != nullptr)
            slopeSink->ZeroVectorSlopes(zeroVectorSlope.Next(idAndIq, electricalSpeed, cosTheta, sinTheta));
        lastElectricalSpeed = electricalSpeed;
        lastIq = idAndIq.q;
        currentStream.Accumulate(idAndIq.d, idAndIq.q, mechanicalAngle);
        const auto& [directSetPoint, quadratureSetPoint] = setPointBuffer.Acquire();
        const auto coggingCurrent = currentCommandActive ? 0.0f : coggingFeedForward.CurrentAt(electricalAngle);
        const auto quadrature = LimitedQuadrature(quadratureSetPoint.Value(), electricalSpeed, coggingCurrent);
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ directSetPoint.Value(), quadrature }, electricalSpeed, busRatio.Ratio() });
        voltageWindow.Accumulate(voltage.q);
        const auto applied = AdvanceRotation(cosTheta, sinTheta, electricalSpeed, motor.dutyDelay);
        return spaceVectorModulator.Generate(park.Inverse(voltage, applied.cosine, applied.sine), busRatio.Inverse());
    }

    float TorqueCascade::MeasureElectricalSpeed(float mechanicalAngle)
    {
        if (!speedMeasurement.previousAngleValid)
        {
            speedMeasurement.previousMechanicalAngle = mechanicalAngle;
            speedMeasurement.previousAngleValid = true;
            return 0.0f;
        }

        const auto sampled = math::WrapToPi(mechanicalAngle - speedMeasurement.previousMechanicalAngle) * speedMeasurement.scale;
        speedMeasurement.previousMechanicalAngle = mechanicalAngle;
        return speedMeasurement.filter.Filter(sampled);
    }
}
