#pragma once

#include <cstddef>

namespace services
{
    class EncoderResolution
    {
    public:
        static constexpr std::size_t minimumSteps{ 8 };
        static constexpr std::size_t maximumWindowSamples{ 256 };

        void Restart(float position);
        void Sample(float position);
        bool Known() const;
        std::size_t WindowSamples(float samplingPeriod) const;

    private:
        float lastPosition{ 0.0f };
        float smallestStep{ 0.0f };
        std::size_t steps{ 0 };
        float lastStep{ 0.0f };
        float stepChangeEnergy{ 0.0f };
        std::size_t stepChanges{ 0 };
        bool started{ false };
    };
}
