#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace state_machine
{
    struct MotorModel
    {
        foc::Ohm resistance{ 0.0f };
        foc::MilliHenry inductance{ 0.0f };
        foc::NewtonMeterSecondSquared inertia{ 0.0f };
        foc::NewtonMeterSecondPerRadian viscousFriction{ 0.0f };

        bool operator==(const MotorModel&) const = default;
    };

    // The resistance is never stored: it follows the winding temperature.
    struct StoredModel
    {
        std::optional<foc::MilliHenry> inductance;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
        std::optional<foc::NewtonMeterSecondPerRadian> viscousFriction;

        bool operator==(const StoredModel&) const = default;
    };

    enum class RecalibrationTrigger : uint8_t
    {
        automatic,
        command
    };

    enum class RecalibrationVerdict : uint8_t
    {
        unchanged,
        applied,
        rejected
    };

    class Recalibration
    {
    public:
        static constexpr float deviationThreshold{ 0.1f };
        static constexpr infra::Duration observationPeriod{ std::chrono::milliseconds(100) };
        static constexpr infra::Duration dwell{ std::chrono::seconds(5) };
        static constexpr std::size_t quantities{ 4 };

        struct Band
        {
            float minimum;
            float maximum;
        };

        // Resistance, inductance, inertia and viscous friction, as ratios to the commissioned value.
        static constexpr std::array<Band, quantities> bands{ { { 0.7f, 2.0f }, { 0.5f, 1.5f }, { 0.5f, 5.0f }, { 0.2f, 5.0f } } };

        struct Outcome
        {
            std::array<RecalibrationVerdict, quantities> verdicts{};
            std::array<float, quantities> readings{};
            uint8_t applied{ 0 };
            uint8_t rejected{ 0 };
        };

        // Returns whether the model in use was reset to the commissioned one with the stored quantities.
        bool Commission(const MotorModel& model, const StoredModel& stored = {});
        bool IsCommissionedFrom(const MotorModel& model, const StoredModel& stored) const;
        // Back to the commissioned model, the resistance included, with nothing stored.
        void Revert();
        void Observe(const OnlineEstimates& estimates);
        Outcome Decide(RecalibrationTrigger trigger);

        const MotorModel& Commissioned() const;
        const MotorModel& InUse() const;
        StoredModel Learned() const;
        uint16_t Applied() const;
        uint16_t Rejected() const;

    private:
        struct Tracked
        {
            float reading{ 0.0f };
            bool settled{ false };
            infra::Duration differing{};
        };

        bool Differs(std::size_t quantity) const;
        RecalibrationVerdict Judge(std::size_t quantity, RecalibrationTrigger trigger);

        MotorModel commissioned;
        StoredModel restored;
        MotorModel inUse;
        std::array<Tracked, quantities> tracked{};
        uint16_t applied{ 0 };
        uint16_t rejected{ 0 };
    };
}
