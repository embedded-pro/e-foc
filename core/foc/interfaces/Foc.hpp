#pragma once

#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"

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

    class FocSpeedTunable
    {
    public:
        virtual void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) = 0;
    };

    class FocOnlineEstimableBase
    {
    public:
        virtual void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) = 0;
        virtual void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) = 0;
    };

    class FocSpeed
        : public FocBase
        , public FocSpeedTunable
        , public FocOnlineEstimableBase
    {
    public:
        virtual void SetPoint(RadiansPerSecond setPoint) = 0;
        virtual hal::Hertz OuterLoopFrequency() const = 0;
    };

    class FocPosition
        : public FocBase
        , public FocSpeedTunable
        , public FocOnlineEstimableBase
    {
    public:
        virtual void SetPoint(Radians setPoint) = 0;
        virtual void SetPositionTunings(const PositionTunings& positionTuning) = 0;
    };
}
