#include "tools/virtual_motor/platform/Pacer.hpp"

namespace virtual_motor
{
    Pacer::Pacer(const infra::Function<TimePoint()>& now, const infra::Function<void()>& advance)
        : now(now)
        , advance(advance)
        , start(now())
        , origin(start)
    {}

    void Pacer::Begin()
    {
        start = now();
        origin = start;
        stepsSinceOrigin = 0;
        stepsRun = 0;
    }

    void Pacer::Service()
    {
        const auto elapsed = now() - origin;
        const auto due = static_cast<uint64_t>(elapsed / step);

        if (due > stepsSinceOrigin + static_cast<uint64_t>(catchUpLimit / step))
        {
            ++overruns;
            origin += step * (due - stepsSinceOrigin);
            return;
        }

        for (; stepsSinceOrigin < due; ++stepsSinceOrigin, ++stepsRun)
            advance();
    }

    void Pacer::Suspend()
    {
        suspendedAt = now();
    }

    void Pacer::Resume()
    {
        const auto suspended = now() - suspendedAt;
        start += suspended;
        origin += suspended;
    }

    Pacer::TimePoint Pacer::NextDue() const
    {
        return origin + step * (stepsSinceOrigin + 1);
    }

    std::chrono::milliseconds Pacer::VirtualTime() const
    {
        return step * stepsRun;
    }

    uint32_t Pacer::Overruns() const
    {
        return overruns;
    }

    float Pacer::RealTimeFactor() const
    {
        const auto wall = std::chrono::duration<float>(now() - start).count();
        if (wall <= 0.0f)
            return 1.0f;

        return std::chrono::duration<float>(VirtualTime()).count() / wall;
    }
}
