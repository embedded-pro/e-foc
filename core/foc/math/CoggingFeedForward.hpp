#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace foc
{
    inline float CatmullRom(float p0, float p1, float p2, float p3, float t)
    {
        return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t + (3.0f * (p1 - p2) + p3 - p0) * t * t * t);
    }

    class CoggingFeedForward
    {
    public:
        static constexpr std::size_t resolution{ 256 };

        void Load(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
        {
            const auto bins = torque.size();
            really_assert(bins >= 4);

            const auto amperesPerCount = torqueScale / torqueConstant.Value();

            for (std::size_t sample = 0; sample != resolution; ++sample)
            {
                const auto position = static_cast<float>(sample * bins) / static_cast<float>(resolution) - 0.5f + static_cast<float>(bins);
                const auto whole = static_cast<std::size_t>(position);
                const auto fraction = position - static_cast<float>(whole);

                const auto p0 = static_cast<float>(torque[(whole + bins - 1) % bins]);
                const auto p1 = static_cast<float>(torque[whole % bins]);
                const auto p2 = static_cast<float>(torque[(whole + 1) % bins]);
                const auto p3 = static_cast<float>(torque[(whole + 2) % bins]);

                current[sample] = amperesPerCount * CatmullRom(p0, p1, p2, p3, fraction);
            }
        }

        void Clear()
        {
            current.fill(0.0f);
        }

        ALWAYS_INLINE_HOT float CurrentAt(float electricalAngle) const
        {
            const auto scaled = electricalAngle * scale;
            const auto truncated = static_cast<int32_t>(scaled);
            const auto floored = truncated - (scaled < static_cast<float>(truncated) ? 1 : 0);
            const auto fraction = scaled - static_cast<float>(floored);
            const auto index = static_cast<std::size_t>(static_cast<uint32_t>(floored)) & mask;
            const auto next = (index + 1) & mask;

            return current[index] + fraction * (current[next] - current[index]);
        }

    private:
        static constexpr std::size_t mask{ resolution - 1 };
        static constexpr float period{ 2.0f * std::numbers::pi_v<float> };
        static constexpr float scale{ static_cast<float>(resolution) / period };

        static_assert((resolution & mask) == 0);

        std::array<float, resolution> current{};
    };
}
