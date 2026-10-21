#include "core/foc/model/CurrentSense.hpp"
#include <algorithm>
#include <cmath>

namespace foc
{
    namespace
    {
        uint32_t AveragedConversions(const CurrentSenseConfig& config)
        {
            return std::max(config.averagedConversions, uint32_t{ 1 });
        }

        float Settled(float elapsed, float timeConstant)
        {
            return timeConstant > 0.0f ? 1.0f - std::exp(-elapsed / timeConstant) : 1.0f;
        }

        float Decayed(float elapsed, float timeConstant)
        {
            return timeConstant > 0.0f ? std::exp(-elapsed / timeConstant) : 0.0f;
        }

        bool Quantised(const CurrentSenseConfig& config)
        {
            return config.fullScaleAmpere > 0.0f && config.resolutionBits > 0;
        }

        float CodeWidth(const CurrentSenseConfig& config)
        {
            return 2.0f * config.fullScaleAmpere / std::ldexp(1.0f, static_cast<int>(config.resolutionBits));
        }

        int64_t Code(const CurrentSenseConfig& config, float ampere)
        {
            const auto highest = static_cast<int64_t>(std::ldexp(1.0f, static_cast<int>(config.resolutionBits))) - 1;
            return std::clamp(static_cast<int64_t>(std::lround((ampere + config.fullScaleAmpere) / CodeWidth(config))), int64_t{ 0 }, highest);
        }

        float Clipped(const CurrentSenseConfig& config, float ampere)
        {
            return config.fullScaleAmpere > 0.0f ? std::clamp(ampere, -config.fullScaleAmpere, config.fullScaleAmpere) : ampere;
        }
    }

    bool CurrentSenseEnabled(const CurrentSenseConfig& config)
    {
        return config.adcClockHertz > 0.0f;
    }

    float ConversionHoldInstant(const CurrentSenseConfig& config, uint32_t conversion)
    {
        const auto clocks = conversion * (config.sampleHoldClocks + config.conversionClocks) + config.sampleHoldClocks;
        return config.triggerDelaySeconds + static_cast<float>(clocks) / config.adcClockHertz;
    }

    float ShuntAmplifierOutput(const CurrentSenseConfig& config, ConductionWindow window, float current, float time)
    {
        if (window.end <= window.start || time < window.start)
            return 0.0f;

        const auto timeConstant = config.amplifierTimeConstantSeconds;

        if (time <= window.end)
            return current * Settled(time - window.start, timeConstant);

        return current * Settled(window.end - window.start, timeConstant) * Decayed(time - window.end, timeConstant);
    }

    float ShuntReading(const CurrentSenseConfig& config, const ShuntSample& sample, uint32_t step, const infra::Function<float()>& conversionNoise)
    {
        const auto count = AveragedConversions(config);
        int64_t codes{ 0 };
        float sum{ 0.0f };

        for (uint32_t conversion = 0; conversion != count; ++conversion)
        {
            const auto time = ConversionHoldInstant(config, step * count + conversion);
            const auto analog = ShuntAmplifierOutput(config, sample.window, sample.current, time) + sample.offset + conversionNoise();

            if (Quantised(config))
                codes += Code(config, analog);
            else
                sum += Clipped(config, analog);
        }

        // Integer division, as the converter's hardware averager drops the remainder.
        if (Quantised(config))
            return static_cast<float>(codes / count) * CodeWidth(config) - config.fullScaleAmpere;

        return sum / static_cast<float>(count);
    }
}
