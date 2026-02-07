#pragma once

#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "source/foc/implementations/FocTorqueImpl.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include <atomic>

namespace foc
{
    class NyquistFactor
    {
    public:
        explicit NyquistFactor(uint8_t factor)
            : value{ factor }
        {
            really_assert(factor > 0 && factor < 20);
        }

        float Value() const
        {
            return value;
        }

    private:
        uint8_t value;
    };

    class FocWithSpeedPidImpl
        : public FieldOrientedControllerSpeedControl
    {
    public:
        FocWithSpeedPidImpl(LowPriorityInterrupt& lowPriorityInterrupt, foc::Ampere maxCurrent, hal::Hertz pwmFrequency, const NyquistFactor& nyquistFactor);

        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void Reset() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& mechanicalAngle) override;

    protected:
        virtual void ExecuteSpeedControlLoop(Radians& position, foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque);
        FocTorqueImpl& FocTorque();

    private:
        RadiansPerSecond FilteredSpeed(Radians currentPosition);
        void ExecuteSpeedLoop(const PhaseCurrents& currentPhases, Radians& mechanicalAngle);

    private:
        FocTorqueImpl focTorqueImpl;
        LowPriorityInterrupt& lowPriorityInterrupt;
        controllers::PidIncrementalSynchronous<float> speedPid;
        Radians previousPosition = Radians{ 0.0f };
        float polePairs;
        std::atomic<NewtonMeter> targetTorque{ NewtonMeter{ 0.0f } };
        uint8_t speedLoopCounter;
        uint8_t nyquistFactor;
        float speedLoopPeriod;
    };
}
