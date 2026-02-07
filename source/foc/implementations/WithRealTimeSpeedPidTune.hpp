#pragma once

#include "foc/interfaces/FieldOrientedController.hpp"
#include "foc/interfaces/FrictionAndInertiaEstimator.hpp"
#include "foc/interfaces/Units.hpp"

namespace foc
{
    class WithRealTimeSpeedPidTune
    {
    public:
        struct Config
        {
            float innovationThreshold{ 0.001f };
            float uncertityThreshold{ 0.1f };
            Volts vdc;
            hal::Hertz speedSamplingFrequency;
        };

        WithRealTimeSpeedPidTune(FieldOrientedControllerSpeedControl& focWithSpeedControl, FrictionAndInertiaEstimator& frictionAndInertiaEstimator, const Config& config);

        void Update(foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque);

    protected:
        FieldOrientedControllerSpeedControl& focWithSpeedPid;
        FrictionAndInertiaEstimator& estimator;

    private:
        foc::SpeedTunings speedTunings;
        float innovationThreshold{ 0.001f };
        float uncertityThreshold{ 0.1f };
        Volts vdc;
        float wc;
    };
}
