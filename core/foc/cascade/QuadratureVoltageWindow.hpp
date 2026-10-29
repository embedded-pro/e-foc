#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    class QuadratureVoltageWindow
    {
    public:
        void Open(uint32_t samplesPerWindow);
        void Close();

        ALWAYS_INLINE_HOT void Accumulate(float quadratureVoltage)
        {
            if (open)
                Add(quadratureVoltage);
        }

        float Mean() const
        {
            return mean;
        }

    private:
        void Add(float quadratureVoltage);

        volatile bool open{ false };
        uint32_t samples{ 1 };
        float inverseSamples{ 1.0f };
        uint32_t count{ 0 };
        float sum{ 0.0f };
        volatile float mean{ 0.0f };
    };
}
