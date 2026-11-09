#pragma once

#include "core/foc/cascade/SpeedLoopDrive.hpp"
#include "foc/interfaces/Foc.hpp"

namespace foc
{
    class SpeedCascade
        : public SpeedLoopDrive<FocSpeed>
        , public SpeedCommandable
        , public SpeedRampable
    {
    public:
        explicit SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 }, uint32_t dutyUpdateDelayPeriods = 0);

        bool ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(RadiansPerSecond point) override;
        void Enable() override;
        void Disable() override;
        hal::Hertz OuterLoopFrequency() const override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) final;
        MotionObservation ObserveMotion() const override;

        void EnableSpeedCommand() override;
        void DisableSpeedCommand() override;
        void CommandSpeed(RadiansPerSecond speed) override;
        hal::Hertz SpeedCommandFrequency() const override;

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;

        void RampSpeed(RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration) override;

    private:
        void LowPriorityHandler();
        bool IsRamped() const;
        void ApplyReference(float value);
        void RampReference();

    private:
        RadiansPerSecond lastSpeedSetPoint{ 0.0f };
        hal::Hertz outerLoopFrequency;
        // Written by the event loop, read by the outer loop; mechanical rad/s per outer-loop period, zero when unlimited.
        volatile float accelerationStep{ 0.0f };
        volatile float decelerationStep{ 0.0f };
        volatile float target{ 0.0f };
        volatile float reference{ 0.0f };
        volatile bool speedCommandActive{ false };
    };
}
