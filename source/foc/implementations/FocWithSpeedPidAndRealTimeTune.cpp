#include "source/foc/implementations/FocWithSpeedPidAndRealTimeTune.hpp"

namespace foc
{
    FocWithSpeedPidAndRealTimeTune::FocWithSpeedPidAndRealTimeTune(LowPriorityInterrupt& lowPriorityInterrupt, foc::Ampere maxCurrent, hal::Hertz pwmFrequency, const NyquistFactor& nyquistFactor)
        : FocWithSpeedPidImpl(lowPriorityInterrupt, maxCurrent, pwmFrequency, nyquistFactor)
        , frictionAndInertiaEstimator(0.99f, hal::Hertz{ 1000 })
        , realTimeSpeedPidTune(*this, frictionAndInertiaEstimator, { 0.001f, 0.1f, foc::Volts{ 24.0f }, hal::Hertz{ 1000 } })
    {}

    void FocWithSpeedPidAndRealTimeTune::ExecuteSpeedControlLoop(Radians& position, foc::PhaseCurrents currentPhases, RadiansPerSecond speed, Radians electricalAngle, foc::NewtonMeter targetTorque)
    {
        realTimeSpeedPidTune.Update(currentPhases, speed, electricalAngle, targetTorque);
        FocWithSpeedPidImpl::ExecuteSpeedControlLoop(position, currentPhases, speed, electricalAngle, targetTorque);
    }
}
