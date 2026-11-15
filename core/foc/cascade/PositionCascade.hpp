#pragma once

#include "core/foc/cascade/PositionProfile.hpp"
#include "core/foc/cascade/SpeedLoopDrive.hpp"
#include "core/foc/cascade/TurnHandover.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "core/foc/math/TurnCounter.hpp"
#include "core/foc/position_loop/PositionControllerSelector.hpp"
#include "foc/interfaces/Foc.hpp"

namespace foc
{
    class PositionCascade
        : public SpeedLoopDrive<FocPosition>
        , public SpeedCommandable
        , public PositionLimitable
        , public QuickStoppable
        , public PositionProfilable
    {
    public:
        explicit PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, CurrentStream& currentStream, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 }, uint32_t dutyUpdateDelayPeriods = 0);

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
        void QuickStop(RadiansPerSecondSquared deceleration) override;
        hal::Hertz SpeedCommandFrequency() const override;

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;

        void LimitTravel(Radians minimum, Radians maximum) override;
        void PositionCircular(bool circular) override;
        void SetPositionTracker(TurnHandover& tracker);
        void ProfileMotion(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration, RadiansPerSecondCubed jerk) override;

    private:
        void LowPriorityHandler();
        void TakeOverTurns();
        void HandBackTurns();
        TravelLimits CurrentLimits() const;
        Radians ObservedPosition() const;

        OPTIMIZE_FOR_SPEED void CountTurns()
        {
            turns.Track(CurrentMechanicalAngle());
            publishedTurns.Write(turns.Count());
            if (tracker != nullptr)
                tracker->Publish(turns.Count());
        }

        OPTIMIZE_FOR_SPEED Radians MeasuredPosition() const
        {
            return Radians{ circular ? turns.Count().angle : PositionOf(turns.Count()) };
        }

        OPTIMIZE_FOR_SPEED PositionControlContext ControlContext(const TravelLimits& limits, const ProfiledReference& reference, float mechanicalSpeed) const
        {
            const auto measured = MeasuredPosition();
            if (!limits.IsOn())
                return { measured, reference.position, RadiansPerSecond{ mechanicalSpeed }, !limits.Folds(), reference.velocity, profile.IsActive() };

            return { measured, limits.Hold(reference.position), RadiansPerSecond{ mechanicalSpeed }, true, reference.velocity, profile.IsActive() };
        }

        Radians ErrorTo(Radians point, Radians measured) const;

        PositionControllerSelector positionLoop;
        hal::Hertz outerLoopFrequency;
        PositionProfile profile;
        // Written by the event loop; each outer-loop tick works on one copy, so its IsOn check guards its clamp.
        TravelLimits travelLimits;
        volatile bool requestedCircular{ false };
        bool circular{ false };
        TurnCounter turns;
        // Written by the outer-loop interrupt for the event loop's observations.
        PublishedTurnCount publishedTurns;
        TurnHandover* tracker{ nullptr };
        RadiansPerSecond speedCommand{ 0.0f };
        bool speedCommandActive{ false };
        volatile bool commandsCurrent{ false };
        bool enabled{ false };
    };
}
