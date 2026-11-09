#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/supervision/SustainedCondition.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"
#include "numerical/math/Math.hpp"
#include <chrono>
#include <optional>

namespace supervision
{
    class OverTemperature
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(100) };
        static constexpr infra::Duration tripHoldOff{ std::chrono::seconds(5) };
        static constexpr infra::Duration filterTimeConstant{ std::chrono::seconds(1) };
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
        std::optional<foc::Celsius> Filtered(const std::optional<foc::Celsius>& measured);
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
        filters::passive::ExponentialMovingAverage<float> filter{ 1.0f - math::Exp(-std::chrono::duration<float>(evaluationPeriod).count() / std::chrono::duration<float>(filterTimeConstant).count()) };
        bool filterSeeded{ false };
        SustainedCondition enabledFor;
    };

    OverTemperature::Settings OverTemperatureSettingsFrom(const services::ConfigData& config);
}
