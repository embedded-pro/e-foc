#pragma once

#include "core/foc/cascade/SpeedLoopDrive.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "core/foc/position_loop/PositionControllerSelector.hpp"
#include "foc/interfaces/Foc.hpp"

namespace foc
{
    class PositionCascade
        : public SpeedLoopDrive<FocPosition>
        , public SpeedCommandable
        , public PositionLimitable
    {
    public:
        explicit PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 }, uint32_t dutyUpdateDelayPeriods = 0);

        bool ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(Radians point) override;
        SelectResult SetPositionTunings(const PositionLoopTunings& tunings) override;
        SelectResult SelectPositionAlgorithm(PositionAlgorithm algorithm) override;
        PositionAlgorithm ActivePositionAlgorithm() const override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) final;
        MotionObservation ObserveMotion() const override;

        void EnableSpeedCommand() override;
        void DisableSpeedCommand() override;
        void CommandSpeed(RadiansPerSecond speed) override;
        hal::Hertz SpeedCommandFrequency() const override;

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;

        void LimitTravel(Radians minimum, Radians maximum) override;

    private:
        void LowPriorityHandler();

        OPTIMIZE_FOR_SPEED PositionControlContext ControlContext(const TravelLimits& limits, float mechanicalSpeed) const
        {
            const Radians measured{ CurrentMechanicalAngle() };
            if (!limits.IsOn())
                return { measured, lastPositionSetPoint, RadiansPerSecond{ mechanicalSpeed } };

            return { measured, limits.Hold(lastPositionSetPoint), RadiansPerSecond{ mechanicalSpeed }, true };
        }

        PositionControllerSelector positionLoop;
        hal::Hertz outerLoopFrequency;
        Radians lastPositionSetPoint{ 0.0f };
        // Written by the event loop; each outer-loop tick works on one copy, so its IsOn check guards its clamp.
        TravelLimits travelLimits;
        RadiansPerSecond speedCommand{ 0.0f };
        bool speedCommandActive{ false };
        volatile bool commandsCurrent{ false };
        bool enabled{ false };
    };
}
