#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace supervision
{
    class EncoderFreezeMonitor
    {
    public:
        struct Limits
        {
            foc::RadiansPerSecondSquared maximumDeceleration{ 0.0f };
            foc::Radians encoderStep{ 0.0f };
        };

        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };
        static constexpr infra::Duration stillConfirmation{ std::chrono::milliseconds(5) };
        static constexpr float minimumSpeed{ 20.0f };

        EncoderFreezeMonitor(drivers::Encoder& encoder, const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Limits()>& limits);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

    private:
        void Evaluate();
        void Disarm();
        void Arm(float reading, infra::TimePoint now);
        void NoteTravel(float reading, float travel, infra::TimePoint now);
        void NoteStill(infra::TimePoint now);
        bool CouldNotHaveStopped() const;
        static bool Exceeds(float speed, float plausible);

        drivers::Encoder& encoder;
        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<Limits()> limits;
        infra::TimerRepeating evaluationTimer;

        Limits armedLimits;
        std::optional<float> anchor;
        infra::TimePoint anchorTime{};
        float lastSpeed{ 0.0f };
        infra::Duration lastSpan{};
        float previousSpeed{ 0.0f };
        infra::Duration previousSpan{};
        std::optional<infra::TimePoint> suspectSince;
        bool still{ false };
        bool reported{ false };
    };

    EncoderFreezeMonitor::Limits LimitsFrom(const services::CalibrationData& calibration, uint32_t encoderResolution, foc::Ampere maxCurrent);
}
