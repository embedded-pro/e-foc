#pragma once

#include "foc/implementations/FocWithSpeedPidImpl.hpp"
#include "foc/implementations/RealTimeFrictionAndInertiaEstimator.hpp"
#include "foc/implementations/WithRealTimeSpeedPidTune.hpp"

namespace foc
{
    class FocWithSpeedPidAndRealTimeTune
        : public FocWithSpeedPidImpl
    {
    public:
        FocWithSpeedPidAndRealTimeTune(LowPriorityInterrupt& lowPriorityInterrupt, foc::Ampere maxCurrent, hal::Hertz pwmFrequency, const NyquistFactor& nyquistFactor);
        void ExecuteSpeedControlLoop(Radians& position, foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque) override;

    private:
        RealTimeFrictionAndInertiaEstimator frictionAndInertiaEstimator;
        WithRealTimeSpeedPidTune realTimeSpeedPidTune;
    };
}
