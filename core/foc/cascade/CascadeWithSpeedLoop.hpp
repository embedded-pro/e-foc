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

    // Double-buffered hand-off from the 20 kHz ISR to the low-priority handler: Publish writes the
    // slot the reader is not on and only then moves the index, so a snapshot can never be read torn.
    class EstimatorChannel
    {
    public:
        ALWAYS_INLINE_HOT void Publish(const EstimatorSnapshot& snapshot)
        {
            const uint8_t writeSlot = 1u - ready;
            slots[writeSlot] = snapshot;
            ready = writeSlot;
        }

        ALWAYS_INLINE_HOT const EstimatorSnapshot& Ready() const
        {
            return slots[ready];
        }

        OnlineMechanicalEstimator* mechanical{ nullptr };
        OnlineElectricalEstimator* electrical{ nullptr };

    private:
        std::array<EstimatorSnapshot, 2> slots{};
        volatile uint8_t ready{ 0 };
    };

    class CascadeWithSpeedLoop
    {
    protected:
        explicit CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency);
        // Non-virtual: cascades live inside a variant and are always destroyed as their concrete type
        ~CascadeWithSpeedLoop();

        void ConfigureImpl(const MotorModelParameters& parameters);
        MechanicalModelParameters ConfigureMechanicsImpl(const MechanicalModelParameters& parameters);
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

        float CurrentMechanicalAngle() const;
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
        bool previousSpeedPositionValid{ false };
        float lastSpeedLoopOutput{ 0.0f };
        float lastElectricalSpeed{ 0.0f };
        RadiansPerSecond speedReference{ 0.0f };
        float speedDt;
        uint32_t prescaler;
        uint32_t triggerCounter{ 0 };
        float polePairs{ 0.0f };
        float vdcInvScale{ 1.0f };
        // Written from command context, read from the control interrupt; it also gates algorithm
        // replacement, which must never run while the interrupt can visit the selector variant.
        volatile bool enabled{ false };

        EstimatorChannel estimators;
    };
}
