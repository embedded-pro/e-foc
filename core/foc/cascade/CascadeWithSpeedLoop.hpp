#pragma once

#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>

namespace foc
{
    namespace detail
    {
        constexpr uint8_t NextFreeSlot(uint8_t slotCount, uint8_t ready, uint8_t held)
        {
            if (ready != held)
                return static_cast<uint8_t>(slotCount * (slotCount - 1u) / 2u - ready - held);

            const auto afterReady = ready == slotCount - 1u ? 0u : ready + 1u;

            return static_cast<uint8_t>(afterReady);
        }
    }

    struct EstimatorSnapshot
    {
        float meanIq{ 0.0f };
        float meanId{ 0.0f };
        float idAtEnd{ 0.0f };
        float meanNormalizedVd{ 0.0f };
        float meanElectricalSpeedTimesIq{ 0.0f };
    };

    class EstimatorWindowAccumulator
    {
    public:
        explicit EstimatorWindowAccumulator(hal::Hertz tickFrequency);

        void Restart();

        ALWAYS_INLINE_HOT void Accumulate(RotatingFrame measured, RotatingFrame normalizedVoltage, float mechanicalAngle, float polePairs)
        {
            const auto electricalRotation = rotationPrimed ? detail::PositionWithWrapAround(mechanicalAngle - previousMechanicalAngle) * polePairs : 0.0f;
            previousMechanicalAngle = mechanicalAngle;
            rotationPrimed = true;

            const auto meanIq = 0.5f * (measured.q + previousIq);

            iqSum += meanIq;
            idSum += 0.5f * (measured.d + previousId);
            vdSum += previousNormalizedVoltage.d + 0.5f * electricalRotation * previousNormalizedVoltage.q;
            rotationTimesIqSum += electricalRotation * meanIq;

            previousIq = measured.q;
            previousId = measured.d;
            previousNormalizedVoltage = normalizedVoltage;
            ++ticks;
        }

        ALWAYS_INLINE_HOT EstimatorSnapshot Close()
        {
            const auto inverseTicks = ticks != 0 ? 1.0f / static_cast<float>(ticks) : 0.0f;
            const EstimatorSnapshot snapshot{ iqSum * inverseTicks, idSum * inverseTicks, previousId, vdSum * inverseTicks, rotationTimesIqSum * inverseTicks * tickFrequency };

            iqSum = 0.0f;
            idSum = 0.0f;
            vdSum = 0.0f;
            rotationTimesIqSum = 0.0f;
            ticks = 0;

            return snapshot;
        }

    private:
        float iqSum{ 0.0f };
        float idSum{ 0.0f };
        float vdSum{ 0.0f };
        float rotationTimesIqSum{ 0.0f };
        float previousIq{ 0.0f };
        float previousId{ 0.0f };
        RotatingFrame previousNormalizedVoltage{};
        float previousMechanicalAngle{ 0.0f };
        bool rotationPrimed{ false };
        float tickFrequency{ 0.0f };
        uint32_t ticks{ 0 };
    };

    class DirectAxisExcitation
    {
    public:
        DirectAxisExcitation(Ampere amplitude, hal::Hertz frequency, hal::Hertz outerLoopFrequency);

        void Restart();
        float Advance();

    private:
        float amplitude;
        uint32_t halfPeriodSamples;
        uint32_t sample{ 0 };
        bool positive{ true };
    };

    class EstimatorChannel
    {
    public:
        ALWAYS_INLINE_HOT void Publish(const EstimatorSnapshot& snapshot)
        {
            slots[writeSlot] = snapshot;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            ready = writeSlot;
            writeSlot = NextFreeSlot();
        }

        void SetMechanical(OnlineMechanicalEstimator& estimator);
        void SetElectrical(OnlineElectricalEstimator& estimator);
        bool HasElectrical() const;
        void UpdateMechanical(float mechanicalSpeed);
        void UpdateElectrical(float vdcInvScale);

    private:
        ALWAYS_INLINE_HOT const EstimatorSnapshot& Acquire()
        {
            held = ready;
            std::atomic_signal_fence(std::memory_order_seq_cst);
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

        ALWAYS_INLINE_HOT void CloseWindow()
        {
            windowAngle = currentAngle;
        }

        float Measure();

    private:
        float samplePeriod;
        volatile float currentAngle{ 0.0f };
        volatile float windowAngle{ 0.0f };
        float previousAngle{ 0.0f };
        bool previousAngleValid{ false };
    };

    class CascadeWithSpeedLoop
    {
    protected:
        explicit CascadeWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency);
        ~CascadeWithSpeedLoop();

        bool ConfigureImpl(const MotorModelParameters& parameters);
        bool ConfigureMechanicsImpl(MechanicalModelParameters& parameters);
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

        void SetDirectCurrentReference(float current);
        float MeasureMechanicalSpeed();

        float CurrentMechanicalAngle() const;
        MotionObservation ObserveSpeedLoopMotion() const;
        float PolePairs() const;
        LowPriorityInterrupt& GetLowPriorityInterrupt();

        void SetOnlineMechanicalEstimatorImpl(OnlineMechanicalEstimator& estimator);
        void SetOnlineElectricalEstimatorImpl(OnlineElectricalEstimator& estimator);
        void UpdateOnlineMechanicalEstimator(float mechanicalSpeed);
        void UpdateOnlineElectricalEstimator();
        void SetDirectAxisExcitation(Ampere amplitude, hal::Hertz frequency);

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
    };
}
