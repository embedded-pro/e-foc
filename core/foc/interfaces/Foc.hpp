#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"

namespace foc
{
    using IdAndIqPoint = std::pair<Ampere, Ampere>;

    class FocBase
    {
    public:
        virtual void Configure(const MotorModelParameters& parameters) = 0;
        virtual void Enable() = 0;
        virtual void Disable() = 0;
        virtual PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) = 0;
    };

    class CurrentLoopTunable
    {
    public:
        virtual void SetCurrentTunings(const CurrentLoopTunings& tunings) = 0;
    };

    class SpeedLoopTunable
    {
    public:
        virtual void ConfigureMechanics(const MechanicalModelParameters& parameters) = 0;
        virtual void SetSpeedTunings(const SpeedLoopTunings& tunings) = 0;
    };

    class PositionLoopTunable
    {
    public:
        virtual void SetPositionTunings(const PositionLoopTunings& tunings) = 0;
    };

    class FocTorque
        : public FocBase
        , public CurrentLoopTunable
    {
    public:
        virtual void SetPoint(IdAndIqPoint setPoint) = 0;
    };

    class FocOnlineEstimableBase
    {
    public:
        virtual void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) = 0;
        virtual void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) = 0;
    };

    class FocSpeed
        : public FocBase
        , public CurrentLoopTunable
        , public SpeedLoopTunable
        , public FocOnlineEstimableBase
    {
    public:
        virtual void SetPoint(RadiansPerSecond setPoint) = 0;
        virtual hal::Hertz OuterLoopFrequency() const = 0;
    };

    class FocPosition
        : public FocBase
        , public CurrentLoopTunable
        , public SpeedLoopTunable
        , public PositionLoopTunable
        , public FocOnlineEstimableBase
    {
    public:
        virtual void SetPoint(Radians setPoint) = 0;
    };
}
