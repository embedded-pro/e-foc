#pragma once

#include <cstddef>
#include <cstdint>

namespace services
{
    // Compares the inertia estimate once per estimator memory; it has settled when two comparisons agree.
    class InertiaSettling
    {
    public:
        void Restart(float forgettingFactor);
        void Track(float inertia, uint32_t observedWindowSamples);
        bool Settled() const;
        float ForgettingFactor() const;

    private:
        float forgettingFactor{ 1.0f };
        float reference{ 0.0f };
        std::size_t updatesSinceReference{ 0 };
        bool hasReference{ false };
        bool settled{ false };
    };
}
