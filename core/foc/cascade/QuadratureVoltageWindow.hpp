#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    class QuadratureVoltageWindow
    {
    public:
        void Open(uint32_t samplesPerWindow)
        {
            samples = samplesPerWindow != 0 ? samplesPerWindow : 1;
            inverseSamples = 1.0f / static_cast<float>(samples);
            sum = 0.0f;
            count = 0;
            mean = 0.0f;
            open = true;
        }

        void Close()
        {
            open = false;
            mean = 0.0f;
        }

        ALWAYS_INLINE_HOT void Accumulate(float quadratureVoltage)
        {
            if (!open)
                return;

            sum += quadratureVoltage;

            if (++count == samples)
            {
                mean = sum * inverseSamples;
                sum = 0.0f;
                count = 0;
            }
        }

        float Mean() const
        {
            return mean;
        }

    private:
        volatile bool open{ false };
        uint32_t samples{ 1 };
        float inverseSamples{ 1.0f };
        uint32_t count{ 0 };
        float sum{ 0.0f };
        volatile float mean{ 0.0f };
    };
}
