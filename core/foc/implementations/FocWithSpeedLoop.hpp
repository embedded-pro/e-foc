#pragma once

#include "core/foc/implementations/FocHelpers.hpp"
#include "core/foc/implementations/SpaceVectorModulation.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/implementations/TrigonometricImpl.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
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

    class FocWithSpeedLoop
    {
    protected:
        explicit FocWithSpeedLoop(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency);

        void SetPolePairsImpl(std::size_t pole);
        void SetCurrentTuningsImpl(Volts Vdc, const IdAndIqTunings& torqueTunings);
        void SetSpeedTuningsImpl(const SpeedTunings& speedTuning);
        void EnableSpeedLoop();
        void DisableSpeedLoop();
        PhasePwmDutyCycles CalculateInnerLoop(const PhaseCurrents& currentPhases, Radians& position);

        controllers::PidIncrementalSynchronous<float>& SpeedPid();
        controllers::PidIncrementalSynchronous<float>& DPid();
        float CurrentMechanicalAngle() const;
        float& PreviousSpeedPosition();
        float& LastSpeedPidOutput();
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
        controllers::PidIncrementalSynchronous<float> speedPid;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        float currentMechanicalAngle{ 0.0f };
        float previousSpeedPosition{ 0.0f };
        float lastSpeedPidOutput{ 0.0f };
        float dt;
        float speedDt;
        uint32_t prescaler;
        uint32_t triggerCounter{ 0 };
        float polePairs{ 0.0f };
        float vdcInvScale{ 1.0f };

        OnlineMechanicalEstimator* onlineMechEstimator{ nullptr };
        OnlineElectricalEstimator* onlineElecEstimator{ nullptr };
        // Double-buffer: ISR writes to snapshots[1 - readyIndex], then publishes
        // by setting readyIndex. The handler reads snapshots[readyIndex]. Since
        // the ISR always writes to the slot the handler is NOT reading, no torn
        // reads are possible regardless of when preemption occurs.
        EstimatorSnapshot snapshots[2];
        volatile uint8_t readyIndex{ 0 };
    };
}
