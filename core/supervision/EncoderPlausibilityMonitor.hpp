#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

namespace supervision
{
    class EncoderPlausibilityMonitor
    {
    public:
        struct Config
        {
            infra::Duration window{};
            float minimumCurrentFraction{};
            foc::RadiansPerSecond minimumDemandedSpeed{};
            foc::Radians minimumPositionError{};
            foc::Radians maximumStillExcursion{};
            bool stallDetection{};
        };

        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(10) };

        EncoderPlausibilityMonitor(drivers::Encoder& encoder, foc::Ampere maxCurrent, const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Config()>& settings);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();
        void Disarm();
        float PushDirection(const foc::MotionObservation& observation) const;
        void NoteTravel(float reading, float push);
        bool StillForWholeWindow(float reading);
        void Report();
        void Restart();

        drivers::Encoder& encoder;
        foc::Ampere maxCurrent;
        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<Config()> settings;
        Config config;
        infra::TimerRepeating evaluationTimer;

        float anchor{ 0.0f };
        infra::Duration stillFor{};
        bool tracking{ false };
        bool reported{ false };
        std::optional<float> travelAnchor;
        bool overpowered{ false };
    };

    EncoderPlausibilityMonitor::Config ConfigFrom(const services::ConfigData& data);
}
