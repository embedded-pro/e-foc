#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <optional>

namespace supervision
{
    class OverTemperature
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(100) };
        // One excitation burst and a margin: until then the estimate may be the one a stop left behind.
        static constexpr infra::Duration tripHoldOff{ std::chrono::seconds(5) };
        static constexpr float reportStep{ 0.01f };

        struct Settings
        {
            foc::Celsius limit;
            foc::Celsius margin;
        };

        using Temperature = infra::Function<std::optional<foc::Celsius>()>;

        OverTemperature(foc::Ampere boardLimit, const infra::Function<void(foc::Ampere)>& onLimit, const infra::Function<void(state_machine::FaultCode)>& onFault);

        void Enable(const infra::Function<Settings()>& settings);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive, const Temperature& temperature);

        foc::Ampere Limit() const;

    private:
        void Evaluate();
        void Derate(const std::optional<foc::Celsius>& temperature, const Settings& current);
        void Trip(bool enabled, const std::optional<foc::Celsius>& temperature, const Settings& current);

        foc::Ampere boardLimit;
        infra::Function<void(foc::Ampere)> onLimit;
        infra::Function<void(state_machine::FaultCode)> onFault;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        Temperature temperature;
        infra::Function<Settings()> settings;
        infra::TimerRepeating evaluationTimer;
        foc::Ampere limit;
        std::optional<infra::TimePoint> enabledSince;
        bool reported{ false };
    };

    OverTemperature::Settings OverTemperatureSettingsFrom(const services::ConfigData& config);
}
