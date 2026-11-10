#pragma once

#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>

namespace virtual_motor
{
    class Pacer
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        static constexpr std::chrono::milliseconds step{ 1 };
        static constexpr std::chrono::milliseconds catchUpLimit{ 50 };

        Pacer(const infra::Function<TimePoint()>& now, const infra::Function<void()>& advance);

        void Service();
        TimePoint NextDue() const;
        std::chrono::milliseconds VirtualTime() const;
        uint32_t Overruns() const;
        float RealTimeFactor() const;

    private:
        infra::Function<TimePoint()> now;
        infra::Function<void()> advance;
        TimePoint start;
        TimePoint origin;
        uint64_t stepsSinceOrigin{ 0 };
        uint64_t stepsRun{ 0 };
        uint32_t overruns{ 0 };
    };
}
