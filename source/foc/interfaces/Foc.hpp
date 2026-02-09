#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include "source/foc/interfaces/Driver.hpp"

namespace foc
{
    using IdAndIqPoint = std::pair<Ampere, Ampere>;
    using IdAndIqTunings = std::pair<controllers::PidTunings<float>, controllers::PidTunings<float>>;
    using SpeedTunings = controllers::PidTunings<float>;
    using PositionTunings = controllers::PidTunings<float>;

    class FocBase
    {
    public:
        virtual void SetPolePairs(std::size_t polePairs) = 0;
        virtual void Enable() = 0;
        virtual void Disable() = 0;
        virtual void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& tunings) = 0;
        virtual PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) = 0;
    };

    class FocTorque
        : public FocBase
    {
    public:
        virtual void SetPoint(IdAndIqPoint setPoint) = 0;
    };

    class FocSpeed
        : public FocBase
    {
    public:
        virtual void SetPoint(RadiansPerSecond setPoint) = 0;
        virtual void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) = 0;
    };
}
