#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    // A drive pushing torque current while it asks the rotor to move expects the encoder to move with it. A
    // reading that stays put for a whole window means the encoder has stopped reporting (or the rotor is
    // locked); either way the loop is regulating a position that no longer tracks the shaft. See REQ-SM-028.
    class EncoderPlausibilityMonitor
    {
    public:
        struct Config
        {
            infra::Duration evaluationPeriod{ std::chrono::milliseconds(10) };
            infra::Duration window{ std::chrono::milliseconds(100) };
            float minimumCurrentFraction{ 0.05f };
            foc::RadiansPerSecond minimumDemandedSpeed{ 2.0f };
            foc::Radians minimumPositionError{ 0.2f };
            foc::Radians maximumStillExcursion{ 0.01f };
        };

        EncoderPlausibilityMonitor(drivers::Encoder& encoder, foc::Ampere maxCurrent, const infra::Function<void()>& onEncoderLoss);

        void Enable(const Config& config);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();
        bool IsPushingForMotion(const foc::MotionObservation& observation) const;
        void Restart();

        drivers::Encoder& encoder;
        foc::Ampere maxCurrent;
        infra::Function<void()> onEncoderLoss;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        Config config;
        infra::TimerRepeating evaluationTimer;

        float anchor{ 0.0f };
        infra::Duration stillFor{};
        bool tracking{ false };
        bool reported{ false };
    };
}
