#pragma once

#include "core/foc/cascade/BusVoltageRatio.hpp"
#include "core/foc/cascade/CurrentStream.hpp"
#include "core/foc/cascade/DirectAxisExcitation.hpp"
#include "core/foc/cascade/EstimatorChannel.hpp"
#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include "core/foc/cascade/QuadratureVoltageWindow.hpp"
#include "core/foc/cascade/SpeedDifferentiator.hpp"
#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/CoggingFeedForward.hpp"
#include "core/foc/math/FrictionFeedForward.hpp"
#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class CascadeWithSpeedLoop
    {
    protected:
        explicit CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods);
        ~CascadeWithSpeedLoop();

        bool ConfigureImpl(const MotorModelParameters& parameters);
        bool ConfigureMechanicsImpl(MechanicalModelParameters& parameters);
        void SetCurrentTuningsImpl(const CurrentLoopTunings& tunings);
        SelectResult SetSpeedTuningsImpl(const SpeedLoopTunings& tunings);
        SelectResult SelectCurrentAlgorithmImpl(CurrentAlgorithm algorithm);
        SelectResult SelectSpeedAlgorithmImpl(SpeedAlgorithm algorithm);
        CurrentAlgorithm ActiveCurrentAlgorithmImpl() const;
        SpeedAlgorithm ActiveSpeedAlgorithmImpl() const;
        void EnableSpeedLoop();
        void DisableSpeedLoop();
        PhasePwmDutyCycles CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position);

        void SetSpeedReference(RadiansPerSecond reference);
        void RunSpeedLoop(float mechanicalSpeed);

        void SetDirectCurrentReference(float current);
        float MeasureMechanicalSpeed();

        void EnableCurrentCommandImpl();
        void CommandCurrentImpl(Ampere quadrature);
        Volts QuadratureVoltageImpl() const;
        bool HoldCommandedCurrent();

        float CurrentMechanicalAngle() const;
        MotionObservation ObserveSpeedLoopMotion() const;
        LowPriorityInterrupt& GetLowPriorityInterrupt();

        void SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator);
        void SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator);
        void UpdateOnlineMechanicalEstimator();
        void UpdateOnlineElectricalEstimator();
        void LoadCoggingMapImpl(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant);
        void ClearCoggingMapImpl();
        void LoadFrictionFeedForwardImpl(NewtonMeter coulomb, NewtonMeter torqueConstant);
        void ClearFrictionFeedForwardImpl();
        void SetCurrentStream(CurrentStream& stream);
        void LimitCurrentImpl(Ampere limit);
        void TrackBusVoltageImpl(Volts measured);

    private:
        struct OuterLoopSchedule
        {
            hal::Hertz frequency;
            uint32_t prescaler;
            uint32_t triggerCounter;
        };

        struct OuterLoopOutputs
        {
            volatile float speedLoopOutput;
            volatile float frictionCurrent;
            volatile float directCurrentReference;
            volatile float speedReference;
            volatile float electricalSpeed;
            volatile float mechanicalSpeed;
        };

        struct CurrentCommand
        {
            volatile float current;
            volatile float quadratureLimit;
            volatile bool active;
            QuadratureVoltageWindow voltageWindow;
        };

        struct MotorScaling
        {
            float polePairs;
            float vdcInvScale;
            float dutyDelay;
        };

        struct Estimation
        {
            EstimatorChannel channel;
            EstimatorWindowAccumulator window;
            volatile float meanIq;
        };

        struct FeedForwards
        {
            CoggingFeedForward cogging;
            FrictionFeedForward friction;
        };

        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        CurrentControllerSelector currentLoop;
        SpeedControllerSelector speedLoop;
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        Ampere maxCurrent;
        OuterLoopSchedule schedule;
        SpeedDifferentiator speedDifferentiator;
        OuterLoopOutputs outer{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        CurrentCommand command;
        MotorScaling scaling{ 0.0f, 1.0f, 0.0f };
        BusVoltageRatio busRatio;
        volatile bool enabled{ false };
        Estimation estimation;
        DirectAxisExcitation directAxisExcitation;
        FeedForwards feedForward;
        CurrentStream* currentStream{ nullptr };
    };
}
