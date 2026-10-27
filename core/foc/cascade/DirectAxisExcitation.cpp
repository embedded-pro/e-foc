#include "core/foc/cascade/DirectAxisExcitation.hpp"
#include <algorithm>

namespace foc
{
    DirectAxisExcitation::DirectAxisExcitation(Ampere amplitude, hal::Hertz frequency, hal::Hertz outerLoopFrequency, std::chrono::seconds burst, std::chrono::seconds cycle)
        : amplitude{ amplitude.Value() }
        , halfPeriodSamples{ frequency.Value() != 0 ? std::max<uint32_t>(outerLoopFrequency.Value() / (2 * frequency.Value()), 1) : 0 }
        , burstSamples{ static_cast<uint32_t>(burst.count()) * outerLoopFrequency.Value() }
        , cycleSamples{ std::max<uint32_t>(static_cast<uint32_t>(cycle.count()) * outerLoopFrequency.Value(), 1) }
    {}

    void DirectAxisExcitation::Restart()
    {
        sample = 0;
        cycleSample = 0;
        positive = true;
    }

    float DirectAxisExcitation::Advance()
    {
        if (halfPeriodSamples == 0)
            return 0.0f;

        if (++cycleSample >= cycleSamples)
            Restart();

        if (cycleSample >= burstSamples)
            return 0.0f;

        if (++sample >= halfPeriodSamples)
        {
            sample = 0;
            positive = !positive;
        }

        return positive ? amplitude : -amplitude;
    }
}
