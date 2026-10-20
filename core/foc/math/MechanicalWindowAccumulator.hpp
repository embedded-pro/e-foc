#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>
#include <cstdint>

namespace foc
{
    class MechanicalWindowAccumulator
    {
    public:
        explicit MechanicalWindowAccumulator(uint32_t samplesPerWindow)
            : samplesPerWindow{ samplesPerWindow }
            , sampleFraction{ 1.0f / static_cast<float>(samplesPerWindow) }
        {}

        void Restart()
        {
            *this = MechanicalWindowAccumulator{ samplesPerWindow };
        }

        // The step is the wrapped mechanical rotation since the previous sample, zero for the first sample.
        ALWAYS_INLINE_HOT void Accumulate(float iq, float mechanicalStep)
        {
            const auto u = (static_cast<float>(samples) + 0.5f) * sampleFraction;
            const auto iqTimesU = iq * u;

            rotation += mechanicalStep;
            positionSum += rotation;
            iqSum += iq;
            iqTimesUSum += iqTimesU;
            iqTimesUSquaredSum += iqTimesU * u;

            const auto size = std::abs(mechanicalStep);
            if (size > 0.0f && (smallestStep == 0.0f || size < smallestStep))
                smallestStep = size;

            ++samples;
        }

        ALWAYS_INLINE_HOT MechanicalWindow Close()
        {
            const auto inverse = samples != 0 ? 1.0f / static_cast<float>(samples) : 0.0f;
            const MechanicalWindow window{ Ampere{ iqSum * inverse }, iqTimesUSum * inverse, iqTimesUSquaredSum * inverse,
                Radians{ positionSum * inverse }, Radians{ rotation }, Radians{ smallestStep }, first };

            rotation = 0.0f;
            positionSum = 0.0f;
            iqSum = 0.0f;
            iqTimesUSum = 0.0f;
            iqTimesUSquaredSum = 0.0f;
            smallestStep = 0.0f;
            samples = 0;
            first = false;

            return window;
        }

    private:
        uint32_t samplesPerWindow;
        float sampleFraction;
        float rotation{ 0.0f };
        float positionSum{ 0.0f };
        float iqSum{ 0.0f };
        float iqTimesUSum{ 0.0f };
        float iqTimesUSquaredSum{ 0.0f };
        float smallestStep{ 0.0f };
        uint32_t samples{ 0 };
        bool first{ true };
    };
}
