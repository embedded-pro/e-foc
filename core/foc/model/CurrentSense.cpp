#include "core/foc/model/CurrentSense.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace foc
{
    namespace
    {
        uint32_t AveragedConversions(const CurrentSenseConfig& config)
        {
            return std::max(config.averagedConversions, uint32_t{ 1 });
        }

        // Past these the result is what the exponential rounds to in float, so it is not computed
        constexpr float settledTimeConstants{ 18.0f };
        constexpr float decayedTimeConstants{ 104.0f };

        float Settled(float elapsed, float timeConstant)
        {
            if (timeConstant <= 0.0f || elapsed >= settledTimeConstants * timeConstant)
                return 1.0f;

            return 1.0f - std::exp(-elapsed / timeConstant);
        }

        float Decayed(float elapsed, float timeConstant)
        {
            if (timeConstant <= 0.0f || elapsed >= decayedTimeConstants * timeConstant)
                return 0.0f;

            return std::exp(-elapsed / timeConstant);
        }

        bool Quantised(const CurrentSenseConfig& config)
        {
            return config.fullScaleAmpere > 0.0f && config.resolutionBits > 0;
        }

        struct Converter
        {
            explicit Converter(const CurrentSenseConfig& config)
                : fullScale(config.fullScaleAmpere)
                , levels(std::ldexp(1.0f, static_cast<int>(config.resolutionBits)))
                , codeWidth(2.0f * config.fullScaleAmpere / levels)
                , highest(static_cast<int32_t>(levels) - 1)
            {}

            int32_t Code(float ampere) const
            {
                return std::clamp(static_cast<int32_t>(std::lround((ampere + fullScale) / codeWidth)), int32_t{ 0 }, highest);
            }

            float fullScale;
            float levels;
            float codeWidth;
            int32_t highest;
        };

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
        const auto quantised = Quantised(config);
        const std::optional<Converter> converter = quantised ? std::optional<Converter>{ std::in_place, config } : std::nullopt;
        int32_t codes{ 0 };
        float sum{ 0.0f };

        for (uint32_t conversion = 0; conversion != count; ++conversion)
        {
            const auto time = ConversionHoldInstant(config, step * count + conversion);
            const auto analog = ShuntAmplifierOutput(config, sample.window, sample.current, time) + sample.offset + conversionNoise();

            if (quantised)
                codes += converter->Code(analog);
            else
                sum += Clipped(config, analog);
        }

        // Integer division, as the converter's hardware averager drops the remainder.
        if (quantised)
            return static_cast<float>(codes / static_cast<int32_t>(count)) * converter->codeWidth - converter->fullScale;

        return sum / static_cast<float>(count);
    }
}
