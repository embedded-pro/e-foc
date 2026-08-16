#pragma once

#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    struct EstimatorSnapshot
    {
        PhaseCurrents phaseCurrents{};
        float electricalAngle{ 0.0f };
        float measuredId{ 0.0f };
        float measuredIq{ 0.0f };
        float normalizedVd{ 0.0f };
    };

    class CascadeWithSpeedLoop
    {
    protected:
        explicit CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency);

        void ConfigureImpl(const MotorModelParameters& parameters);
        void ConfigureMechanicsImpl(const MechanicalModelParameters& parameters);
        void SetCurrentTuningsImpl(const CurrentLoopTunings& tunings);
        void SetSpeedTuningsImpl(const SpeedLoopTunings& tunings);
        SelectResult SelectCurrentAlgorithmImpl(CurrentAlgorithm algorithm);
        SelectResult SelectSpeedAlgorithmImpl(SpeedAlgorithm algorithm);
        CurrentAlgorithm ActiveCurrentAlgorithmImpl() const;
        SpeedAlgorithm ActiveSpeedAlgorithmImpl() const;
        void EnableSpeedLoop();
        void DisableSpeedLoop();
        PhasePwmDutyCycles CalculateInnerLoop(const PhaseCurrents& currentPhases, const Radians& position);

        void SetSpeedReference(RadiansPerSecond reference);
        void RunSpeedLoop(float mechanicalSpeed);
        // Lets an outer law drive the current loop itself, bypassing the speed loop entirely
        void SetDirectCurrentReference(float current);
        float MeasureMechanicalSpeed();

        CurrentControllerSelector& CurrentLoop();
        SpeedControllerSelector& SpeedLoop();
        float CurrentMechanicalAngle() const;
        float SpeedDt() const;
        float PolePairs() const;
        LowPriorityInterrupt& GetLowPriorityInterrupt();

        void SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator);
        void SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator);
        void UpdateOnlineMechanicalEstimator(float mechanicalSpeed);
        void UpdateOnlineElectricalEstimator(float electricalSpeed);

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        CurrentControllerSelector currentLoop;
        SpeedControllerSelector speedLoop;
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        Ampere maxCurrent;
        hal::Hertz outerLoopFrequency;
        float currentMechanicalAngle{ 0.0f };
        float previousSpeedPosition{ 0.0f };
        float lastSpeedLoopOutput{ 0.0f };
        float lastElectricalSpeed{ 0.0f };
        RadiansPerSecond speedReference{ 0.0f };
        float speedDt;
        uint32_t prescaler;
        uint32_t triggerCounter{ 0 };
        float polePairs{ 0.0f };
        float vdcInvScale{ 1.0f };
        bool enabled{ false };

        OnlineMechanicalEstimator* onlineMechEstimator{ nullptr };
        OnlineElectricalEstimator* onlineElecEstimator{ nullptr };
        // Double-buffer: ISR writes to snapshots[1 - readyIndex], then publishes
        // by setting readyIndex. The handler reads snapshots[readyIndex]. Since
        // the ISR always writes to the slot the handler is NOT reading, no torn
        // reads are possible regardless of when preemption occurs.
        std::array<EstimatorSnapshot, 2> snapshots{};
        volatile uint8_t readyIndex{ 0 };
    };
}
