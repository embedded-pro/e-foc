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
#include <atomic>

namespace foc
{
    namespace detail
    {
        constexpr uint8_t NextFreeSlot(uint8_t slotCount, uint8_t ready, uint8_t held)
        {
            return ready == held
                       ? static_cast<uint8_t>(ready == slotCount - 1u ? 0u : ready + 1u)
                       : static_cast<uint8_t>(slotCount * (slotCount - 1u) / 2u - ready - held);
        }
    }

    struct EstimatorSnapshot
    {
        PhaseCurrents phaseCurrents{};
        float electricalAngle{ 0.0f };
        float measuredId{ 0.0f };
        float measuredIq{ 0.0f };
        float normalizedVd{ 0.0f };
    };

    class EstimatorChannel
    {
    public:
        ALWAYS_INLINE_HOT void Publish(const EstimatorSnapshot& snapshot)
        {
            slots[writeSlot] = snapshot;
            std::atomic_signal_fence(std::memory_order_release);
            ready = writeSlot;
            writeSlot = NextFreeSlot();
        }

        void SetMechanical(OnlineMechanicalEstimator& estimator);
        void SetElectrical(OnlineElectricalEstimator& estimator);
        void UpdateMechanical(float mechanicalSpeed);
        void UpdateElectrical(float electricalSpeed, float vdcInvScale);

    private:
        ALWAYS_INLINE_HOT const EstimatorSnapshot& Acquire()
        {
            held = ready;
            std::atomic_signal_fence(std::memory_order_acquire);
            return slots[held];
        }

        ALWAYS_INLINE_HOT uint8_t NextFreeSlot() const
        {
            return detail::NextFreeSlot(slotCount, ready, held);
        }

        static constexpr uint8_t slotCount = 3;

        OnlineMechanicalEstimator* mechanical{ nullptr };
        OnlineElectricalEstimator* electrical{ nullptr };
        std::array<EstimatorSnapshot, slotCount> slots{};
        uint8_t writeSlot{ 1 };
        volatile uint8_t ready{ 0 };
        volatile uint8_t held{ 0 };
    };

    class SpeedDifferentiator
    {
    public:
        explicit SpeedDifferentiator(hal::Hertz outerLoopFrequency);

        void Restart();

        ALWAYS_INLINE_HOT void Track(float mechanicalAngle)
        {
            currentAngle = mechanicalAngle;
        }

        ALWAYS_INLINE_HOT float CurrentAngle() const
        {
            return currentAngle;
        }

        float Measure();

    private:
        float samplePeriod;
        volatile float currentAngle{ 0.0f };
        float previousAngle{ 0.0f };
        bool previousAngleValid{ false };
    };

    class CascadeWithSpeedLoop
    {
    protected:
        explicit CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency);
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
        SpeedDifferentiator speedDifferentiator;
        volatile float lastSpeedLoopOutput{ 0.0f };
        volatile float lastElectricalSpeed{ 0.0f };
        volatile float speedReference{ 0.0f };
        uint32_t prescaler;
        uint32_t triggerCounter{ 0 };
        float polePairs{ 0.0f };
        float vdcInvScale{ 1.0f };
        volatile bool enabled{ false };

        EstimatorChannel estimators;
    };
}
