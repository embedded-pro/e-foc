#pragma once

#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "source/foc/implementations/SpaceVectorModulation.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"

namespace foc
{
    class FocPositionImpl
        : public FocPosition
    {
    public:
        explicit FocPositionImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 });

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(Radians point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void SetPositionTunings(const PositionTunings& positionTuning) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

    private:
        void LowPriorityHandler();

    private:
        [[no_unique_address]] Park park;
        [[no_unique_address]] Clarke clarke;
        controllers::PidIncrementalSynchronous<float> positionPid;
        controllers::PidIncrementalSynchronous<float> speedPid;
        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        [[no_unique_address]] SpaceVectorModulation spaceVectorModulator;
        LowPriorityInterrupt& lowPriorityInterrupt;
        float currentMechanicalAngle = 0.0f;
        float previousSpeedPosition = 0.0f;
        float lastSpeedPidOutput = 0.0f;
        float dt;
        float speedDt;
        uint32_t prescaler;
        uint32_t triggerCounter = 0;
        float polePairs = 0.0f;
        Radians lastPositionSetPoint{ 0.0f };
    };
}
