#include "core/supervision/MotorOverload.hpp"
#include <algorithm>
#include <cmath>

namespace supervision
{
    namespace
    {
        constexpr float evaluationSeconds = std::chrono::duration<float>(MotorOverload::evaluationPeriod).count();

        float ThermalTimeConstant(float continuous, float peak, float peakSeconds)
        {
            return peakSeconds / -std::log(1.0f - (continuous * continuous) / (peak * peak));
        }
    }

    MotorOverload::MotorOverload(foc::Ampere boardLimit, const infra::Function<void(foc::Ampere)>& onLimit)
        : boardLimit{ boardLimit }
        , onLimit{ onLimit }
        , applied{ boardLimit }
    {}

    void MotorOverload::Enable(const infra::Function<Ratings()>& ratings)
    {
        this->ratings = ratings;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void MotorOverload::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
    }

    float MotorOverload::Load() const
    {
        return continuousSquared > 0.0f ? squaredCurrentState / continuousSquared : 0.0f;
    }

    foc::Ampere MotorOverload::Limit() const
    {
        return applied;
    }

    void MotorOverload::Evaluate()
    {
        const auto rated = ratings();
        const auto peak = rated.peak.Value() > 0.0f ? std::min(rated.peak.Value(), boardLimit.Value()) : boardLimit.Value();
        const auto continuous = std::min(rated.continuous.Value(), peak);

        if (continuous <= 0.0f)
        {
            squaredCurrentState = 0.0f;
            continuousSquared = 0.0f;
            foldedBack = false;
            Apply(boardLimit);
            return;
        }

        if (continuous >= peak || rated.peakTime <= infra::Duration::zero())
        {
            continuousSquared = continuous * continuous;
            Apply(foc::Ampere{ continuous });
            return;
        }

        Track(Ratings{ foc::Ampere{ continuous }, foc::Ampere{ peak }, rated.peakTime }, MeasuredCurrent());
        Apply(foldedBack ? foc::Ampere{ continuous } : foc::Ampere{ peak });
    }

    float MotorOverload::MeasuredCurrent() const
    {
        return activeDrive != nullptr ? activeDrive().ObserveMotion().measuredTorqueCurrent.Value() : 0.0f;
    }

    void MotorOverload::Track(const Ratings& effective, float current)
    {
        const auto continuous = effective.continuous.Value();
        const auto timeConstant = ThermalTimeConstant(continuous, effective.peak.Value(), std::chrono::duration<float>(effective.peakTime).count());

        continuousSquared = continuous * continuous;
        squaredCurrentState += (current * current - squaredCurrentState) * (1.0f - std::exp(-evaluationSeconds / timeConstant));

        if (!foldedBack && squaredCurrentState >= continuousSquared)
            foldedBack = true;
        else if (foldedBack && squaredCurrentState < releaseFraction * continuousSquared)
            foldedBack = false;
    }

    void MotorOverload::Apply(foc::Ampere limit)
    {
        if (limit == applied)
            return;

        applied = limit;
        onLimit(limit);
    }

    MotorOverload::Ratings OverloadRatingsFrom(const services::ConfigData& config)
    {
        const auto peakTime = std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(config.limits.peakCurrentTime));
        return { foc::Ampere{ config.limits.continuousCurrent }, foc::Ampere{ config.limits.peakCurrent }, peakTime };
    }
}
