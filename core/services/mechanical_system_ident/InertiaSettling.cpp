#include "core/services/mechanical_system_ident/InertiaSettling.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr float settlingTolerance{ 0.1f };
        constexpr std::size_t maximumSettlingInterval{ 2000 };
        constexpr std::size_t minimumSettlingUpdates{ 8 };
    }

    void InertiaSettling::Restart(float forgettingFactor)
    {
        this->forgettingFactor = forgettingFactor;
        hasReference = false;
        settled = false;
        updatesSinceReference = 0;
    }

    void InertiaSettling::Track(float inertia, uint32_t observedWindowSamples)
    {
        const auto memoryInSamples = forgettingFactor < 1.0f ? math::Ceil(1.0f / (1.0f - forgettingFactor)) : static_cast<float>(maximumSettlingInterval);
        const auto memoryInUpdates = std::max<std::size_t>(minimumSettlingUpdates, static_cast<std::size_t>(math::Ceil(memoryInSamples / static_cast<float>(observedWindowSamples))));

        if (++updatesSinceReference < memoryInUpdates)
            return;

        settled = hasReference && math::Abs(inertia - reference) <= settlingTolerance * math::Abs(inertia);
        reference = inertia;
        hasReference = true;
        updatesSinceReference = 0;
    }

    bool InertiaSettling::Settled() const
    {
        return settled;
    }

    float InertiaSettling::ForgettingFactor() const
    {
        return forgettingFactor;
    }
}
