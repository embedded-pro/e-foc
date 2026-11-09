#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/supervision/I2tThermalModel.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace supervision
{
    class MotorOverload
    {
    public:
        static constexpr infra::Duration evaluationPeriod{ std::chrono::milliseconds(1) };
        static constexpr float releaseFraction{ 0.5f };

        struct Ratings
        {
            foc::Ampere continuous;
            foc::Ampere peak;
            infra::Duration peakTime;
        };

        MotorOverload(foc::Ampere boardLimit, const infra::Function<void(foc::Ampere)>& onLimit);

        void Enable(const infra::Function<Ratings()>& ratings);
        void Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive);

        float Load() const;
        foc::Ampere Limit() const;

    private:
        void Evaluate();
        float MeasuredCurrent() const;
        void Track(const Ratings& effective, float current);
        void Apply(foc::Ampere limit);

        foc::Ampere boardLimit;
        infra::Function<void(foc::Ampere)> onLimit;
        infra::Function<const state_machine::FocStateMachineBase&()> activeDrive;
        infra::Function<Ratings()> ratings;
        infra::TimerRepeating evaluationTimer;
        I2tThermalModel thermalModel;
        float continuousSquared{ 0.0f };
        bool foldedBack{ false };
        foc::Ampere applied;
    };

    MotorOverload::Ratings OverloadRatingsFrom(const services::ConfigData& config);
}
