#pragma once

#include "core/foc/cascade/BusVoltageRatio.hpp"
#include "core/foc/cascade/CurrentStream.hpp"
#include "core/foc/cascade/QuadratureVoltageWindow.hpp"
#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/CoggingFeedForward.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/foc/transforms/ZeroVectorSlope.hpp"
#include "infra/util/TripleBuffer.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"

namespace foc
{
    class TorqueCascade
        : public FocTorque
        , public CurrentCommandable
        , public SpeedLimitable
        , public CurrentLimitable
        , public CoggingCompensable
        , public BusVoltageTrackable
    {
    public:
        explicit TorqueCascade(foc::Ampere maxCurrent, CurrentStream& currentStream);

        bool Configure(const MotorModelParameters& parameters) override;
        void SetPoint(IdAndIqPoint setPoint) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override;
        CurrentAlgorithm ActiveCurrentAlgorithm() const override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) final;

        Ampere MeasuredQuadratureCurrent() const final
        {
            return Ampere{ lastIq };
        }

        MotionObservation ObserveMotion() const override;
        void SetZeroVectorSlopeSink(ZeroVectorSlopeSink& sink);

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;
        void CommandCurrent(Ampere quadrature) override;
        Volts QuadratureVoltage() const override;

        void LimitSpeed(RadiansPerSecond limit) override;
        void LimitCurrent(Ampere limit) override;
        void TrackBusVoltage(Volts measured) override;
        void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) override;
        void ClearCoggingMap() override;

    private:
        float MeasureElectricalSpeed(float mechanicalAngle);
        void UpdateSpeedLimit();
        IdAndIqPoint LimitToCurrentEnvelope(IdAndIqPoint setPoint) const;
        float LimitedQuadrature(float quadrature, float electricalSpeed, float feedForward) const;

        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        CurrentControllerSelector currentLoop;
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        Ampere maxCurrent;

        struct MotorConstants
        {
            float polePairs{ 0.0f };
            float dutyDelay{ 0.0f };
        };

        MotorConstants motor;
        volatile bool enabled{ false };
        IdAndIqPoint requestedSetPoint{ Ampere{ 0.0f }, Ampere{ 0.0f } };
        infra::TripleBuffer<IdAndIqPoint> setPointBuffer;

        struct SpeedMeasurement
        {
            float scale{ 0.0f };
            filters::passive::ExponentialMovingAverage<float> filter{ 1.0f };
            float previousMechanicalAngle{ 0.0f };
            bool previousAngleValid{ false };
        };

        struct DriveLimits
        {
            RadiansPerSecond speed{ 0.0f };
            volatile float inverseElectricalSpeed{ 0.0f };
            volatile float current{ 0.0f };
        };

        SpeedMeasurement speedMeasurement;
        QuadratureVoltageWindow voltageWindow;
        float voltageScale{ 0.0f };
        uint32_t samplesPerMillisecond{ 1 };
        volatile float lastElectricalSpeed{ 0.0f };
        volatile float lastIq{ 0.0f };
        DriveLimits limits;
        BusVoltageRatio busRatio;
        CoggingFeedForward coggingFeedForward;
        ZeroVectorSlope zeroVectorSlope;
        ZeroVectorSlopeSink* slopeSink{ nullptr };
        volatile bool currentCommandActive{ false };
        CurrentStream& currentStream;
    };
}
