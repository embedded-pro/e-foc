#include "core/foc/model/PowerStage.hpp"
#include <algorithm>

namespace foc
{
    namespace
    {
        struct Rails
        {
            float low;
            float high;
        };

        float FloatingArea(const PowerStageConfig& config, float start, float current, float duration, Rails rails)
        {
            if (duration <= 0.0f || current == 0.0f)
                return start * std::max(duration, 0.0f);

            const auto clamp = current > 0.0f ? rails.low : rails.high;

            if (config.switchNodeCapacitanceFarad <= 0.0f)
                return clamp * duration;

            const auto slope = -current / config.switchNodeCapacitanceFarad;
            const auto reach = (clamp - start) / slope;

            if (reach >= duration)
                return start * duration + 0.5f * slope * duration * duration;

            return start * reach + 0.5f * slope * reach * reach + clamp * (duration - reach);
        }
    }

    float PowerStageLegVoltage(const PowerStageConfig& config, float duty, float current, float busVoltage, float period)
    {
        const auto d = std::clamp(duty, 0.0f, 1.0f);
        const auto deadTime = config.deadTimeSeconds;

        if (deadTime <= 0.0f && config.onResistanceOhm == 0.0f)
            return d * busVoltage;

        const auto conductionDrop = current * config.onResistanceOhm;

        if (d <= 0.0f)
            return -conductionDrop;

        if (d >= 1.0f)
            return busVoltage - conductionDrop;

        const Rails rails{ -config.diodeForwardVolts, busVoltage + config.diodeForwardVolts };
        const auto pulse = d * period;
        const auto highTime = pulse - deadTime;
        const auto lowTime = period - pulse - deadTime;

        if (highTime < config.minimumPulseSeconds)
        {
            const auto floating = std::min(pulse + deadTime, period);
            return (FloatingArea(config, 0.0f, current, floating, rails) - conductionDrop * (period - floating)) / period;
        }

        if (lowTime < config.minimumPulseSeconds)
        {
            const auto floating = std::min(period - pulse + deadTime, period);
            return (FloatingArea(config, busVoltage, current, floating, rails) + (busVoltage - conductionDrop) * (period - floating)) / period;
        }

        const auto rising = FloatingArea(config, 0.0f, current, deadTime, rails);
        const auto falling = FloatingArea(config, busVoltage, current, deadTime, rails);

        return ((busVoltage - conductionDrop) * highTime - conductionDrop * lowTime + rising + falling) / period;
    }

    ConductionWindow LowSideConduction(const PowerStageConfig& config, float duty, float current, float period)
    {
        const auto d = std::clamp(duty, 0.0f, 1.0f);
        const auto deadTime = std::max(config.deadTimeSeconds, 0.0f);
        const auto diodeConducts = current > 0.0f;
        const auto highTime = d * period - deadTime;
        const auto lowTime = period - d * period - deadTime;

        if (d <= 0.0f || (diodeConducts && highTime < config.minimumPulseSeconds))
            return { -period, period };

        if (d >= 1.0f)
            return {};

        if (lowTime < config.minimumPulseSeconds)
        {
            const auto halfFloating = 0.5f * std::min(period - d * period + deadTime, period);
            return diodeConducts ? ConductionWindow{ -halfFloating, halfFloating } : ConductionWindow{};
        }

        const auto half = 0.5f * lowTime + (diodeConducts ? deadTime : 0.0f);
        return { -half, half };
    }
}
