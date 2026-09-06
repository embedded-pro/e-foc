#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
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
        virtual ~FocBase() = default;

        virtual void Configure(const MotorModelParameters& parameters) = 0;
        virtual void Enable() = 0;
        virtual void Disable() = 0;
        virtual PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) = 0;
    };

    class CurrentLoopTunable
    {
    public:
        virtual ~CurrentLoopTunable() = default;

        virtual void SetCurrentTunings(const CurrentLoopTunings& tunings) = 0;
    };

    // Selection is a configuration-time operation; implementations reject it while enabled.
    class CurrentLoopSelectable
    {
    public:
        virtual ~CurrentLoopSelectable() = default;

        virtual SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) = 0;
        virtual CurrentAlgorithm ActiveCurrentAlgorithm() const = 0;
    };

    class SpeedLoopSelectable
    {
    public:
        virtual ~SpeedLoopSelectable() = default;

        virtual SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) = 0;
        virtual SpeedAlgorithm ActiveSpeedAlgorithm() const = 0;
    };

    class SpeedLoopTunable
    {
    public:
        virtual ~SpeedLoopTunable() = default;

        virtual void ConfigureMechanics(const MechanicalModelParameters& parameters) = 0;
        virtual void SetSpeedTunings(const SpeedLoopTunings& tunings) = 0;
    };

    // Reports rejection because the state feedback laws cannot always be redesigned for a
    // new set of tunings, and implementations refuse retuning outright while the motor is enabled.
    class PositionLoopTunable
    {
    public:
        virtual ~PositionLoopTunable() = default;

        virtual SelectResult SetPositionTunings(const PositionLoopTunings& tunings) = 0;
    };

    class PositionLoopSelectable
    {
    public:
        virtual ~PositionLoopSelectable() = default;

        virtual SelectResult SelectPositionAlgorithm(PositionAlgorithm algorithm) = 0;
        virtual PositionAlgorithm ActivePositionAlgorithm() const = 0;
    };

    class FocTorque
        : public FocBase
        , public CurrentLoopTunable
        , public CurrentLoopSelectable
    {
    public:
        virtual ~FocTorque() = default;

        virtual void SetPoint(IdAndIqPoint setPoint) = 0;
    };

    class FocOnlineEstimableBase
    {
    public:
        virtual ~FocOnlineEstimableBase() = default;

        virtual void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) = 0;
        virtual void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) = 0;
    };

    class SpeedCommandable
    {
    public:
        virtual ~SpeedCommandable() = default;

        virtual void EnableSpeedCommand() = 0;
        virtual void DisableSpeedCommand() = 0;
        virtual void CommandSpeed(RadiansPerSecond speed) = 0;
        virtual hal::Hertz SpeedCommandFrequency() const = 0;
    };

    class FocSpeed
        : public FocBase
        , public CurrentLoopTunable
        , public CurrentLoopSelectable
        , public SpeedLoopTunable
        , public SpeedLoopSelectable
        , public FocOnlineEstimableBase
    {
    public:
        virtual ~FocSpeed() = default;

        virtual void SetPoint(RadiansPerSecond setPoint) = 0;
        virtual hal::Hertz OuterLoopFrequency() const = 0;
    };

    class FocPosition
        : public FocBase
        , public CurrentLoopTunable
        , public CurrentLoopSelectable
        , public SpeedLoopTunable
        , public SpeedLoopSelectable
        , public PositionLoopTunable
        , public PositionLoopSelectable
        , public FocOnlineEstimableBase
    {
    public:
        virtual ~FocPosition() = default;

        virtual void SetPoint(Radians setPoint) = 0;
    };
}
