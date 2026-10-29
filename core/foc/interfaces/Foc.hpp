#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace foc
{
    using IdAndIqPoint = std::pair<Ampere, Ampere>;

    struct MotionObservation
    {
        RadiansPerSecond measuredSpeed{ 0.0f };
        Ampere measuredTorqueCurrent{ 0.0f };
        RadiansPerSecond demandedSpeed{ 0.0f };
        Radians positionError{ 0.0f };
    };

    class FocBase
    {
    protected:
        ~FocBase() = default;

    public:
        virtual MotionObservation ObserveMotion() const = 0;

        virtual bool Configure(const MotorModelParameters& parameters) = 0;
        virtual void Enable() = 0;
        virtual void Disable() = 0;
        virtual PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) = 0;
    };

    class CurrentLoopTunable
    {
    protected:
        ~CurrentLoopTunable() = default;

    public:
        virtual void SetCurrentTunings(const CurrentLoopTunings& tunings) = 0;
    };

    class CurrentLoopSelectable
    {
    protected:
        ~CurrentLoopSelectable() = default;

    public:
        virtual SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) = 0;
        virtual CurrentAlgorithm ActiveCurrentAlgorithm() const = 0;
    };

    class SpeedLoopSelectable
    {
    protected:
        ~SpeedLoopSelectable() = default;

    public:
        virtual SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) = 0;
        virtual SpeedAlgorithm ActiveSpeedAlgorithm() const = 0;
    };

    class SpeedLoopTunable
    {
    protected:
        ~SpeedLoopTunable() = default;

    public:
        virtual bool ConfigureMechanics(const MechanicalModelParameters& parameters) = 0;
        virtual SelectResult SetSpeedTunings(const SpeedLoopTunings& tunings) = 0;
    };

    class PositionLoopTunable
    {
    protected:
        ~PositionLoopTunable() = default;

    public:
        virtual SelectResult SetPositionTunings(const PositionLoopTunings& tunings) = 0;
    };

    class PositionLoopSelectable
    {
    protected:
        ~PositionLoopSelectable() = default;

    public:
        virtual SelectResult SelectPositionAlgorithm(PositionAlgorithm algorithm) = 0;
        virtual PositionAlgorithm ActivePositionAlgorithm() const = 0;
    };

    class FocTorque
        : public FocBase
        , public CurrentLoopTunable
        , public CurrentLoopSelectable
    {
    protected:
        ~FocTorque() = default;

    public:
        virtual void SetPoint(IdAndIqPoint setPoint) = 0;
    };

    class FocOnlineEstimableBase
    {
    protected:
        ~FocOnlineEstimableBase() = default;

    public:
        virtual void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) = 0;
        virtual void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) = 0;
    };

    class CoggingCompensable
    {
    protected:
        ~CoggingCompensable() = default;

    public:
        virtual void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) = 0;
        virtual void ClearCoggingMap() = 0;
    };

    class SpeedCommandable
    {
    protected:
        ~SpeedCommandable() = default;

    public:
        virtual void EnableSpeedCommand() = 0;
        virtual void DisableSpeedCommand() = 0;
        virtual void CommandSpeed(RadiansPerSecond speed) = 0;
        virtual hal::Hertz SpeedCommandFrequency() const = 0;
    };

    class CurrentCommandable
    {
    protected:
        ~CurrentCommandable() = default;

    public:
        virtual void EnableCurrentCommand() = 0;
        virtual void DisableCurrentCommand() = 0;
        virtual void CommandCurrent(Ampere quadrature) = 0;
        virtual Volts QuadratureVoltage() const = 0;
    };

    class FocSpeed
        : public FocBase
        , public CurrentLoopTunable
        , public CurrentLoopSelectable
        , public SpeedLoopTunable
        , public SpeedLoopSelectable
        , public FocOnlineEstimableBase
    {
    protected:
        ~FocSpeed() = default;

    public:
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
    protected:
        ~FocPosition() = default;

    public:
        virtual void SetPoint(Radians setPoint) = 0;
    };
}
