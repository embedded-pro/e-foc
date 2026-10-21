#pragma once

#include "core/foc/model/PowerStage.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace foc
{
    struct CurrentSenseConfig
    {
        float adcClockHertz{ 0.0f };
        uint32_t sampleHoldClocks{ 0 };
        uint32_t conversionClocks{ 0 };
        uint32_t averagedConversions{ 0 };
        float triggerDelaySeconds{ 0.0f };
        float amplifierTimeConstantSeconds{ 0.0f };
        float fullScaleAmpere{ 0.0f };
        uint32_t resolutionBits{ 0 };
    };

    struct ShuntSample
    {
        ConductionWindow window;
        float current;
        float offset;
    };

    bool CurrentSenseEnabled(const CurrentSenseConfig& config);
    float ConversionHoldInstant(const CurrentSenseConfig& config, uint32_t conversion);
    float ShuntAmplifierOutput(const CurrentSenseConfig& config, ConductionWindow window, float current, float time);

    // What the ADC reports for one step of a sequence that converts the phases one after another from a trigger at
    // the centre of the low-side pulse, averaging each step over consecutive conversions; see Part M of
    // documentation/design/software-in-the-loop.md.
    float ShuntReading(const CurrentSenseConfig& config, const ShuntSample& sample, uint32_t step, const infra::Function<float()>& conversionNoise);
}
