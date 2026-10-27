#pragma once

#include "core/foc/cascade/DirectAxisExcitation.hpp"
#include "core/foc/cascade/EstimatorChannel.hpp"
#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include "core/foc/cascade/SpeedDifferentiator.hpp"
#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/CoggingFeedForward.hpp"
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

        float CurrentMechanicalAngle() const;
        MotionObservation ObserveSpeedLoopMotion() const;
        float PolePairs() const;
        LowPriorityInterrupt& GetLowPriorityInterrupt();

        void SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator);
        void SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator);
        void UpdateOnlineMechanicalEstimator();
        void UpdateOnlineElectricalEstimator();
        void SetDirectAxisExcitation(Ampere amplitude, hal::Hertz frequency);
        void LoadCoggingMapImpl(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant);
        void ClearCoggingMapImpl();

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        CurrentControllerSelector currentLoop;
        SpeedControllerSelector speedLoop;
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        Ampere maxCurrent;
        hal::Hertz outerLoopFrequency;
        SpeedDifferentiator speedDifferentiator;
        volatile float lastSpeedLoopOutput{ 0.0f };
        volatile float directCurrentReference{ 0.0f };
        volatile float lastElectricalSpeed{ 0.0f };
        volatile float lastMechanicalSpeed{ 0.0f };
        volatile float lastMeanIq{ 0.0f };
        volatile float speedReference{ 0.0f };
        uint32_t prescaler;
        uint32_t triggerCounter{ 0 };
        float polePairs{ 0.0f };
        float vdcInvScale{ 1.0f };
        volatile bool enabled{ false };

        EstimatorChannel estimators;
        EstimatorWindowAccumulator estimatorWindow;
        DirectAxisExcitation directAxisExcitation;
        CoggingFeedForward coggingFeedForward;
    };
}
