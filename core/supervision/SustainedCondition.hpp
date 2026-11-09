#pragma once

#include "infra/timer/Timer.hpp"
#include <cstdint>
#include <optional>

namespace supervision
{
    class SustainedCondition
    {
    public:
        bool Held(bool condition, infra::TimePoint now, infra::Duration required);
        bool HeldCount(bool condition, uint32_t required);
        bool ReportOnce(bool trigger);
        bool Reported() const;
        void Reset();

    private:
        std::optional<infra::TimePoint> since;
        uint32_t count{ 0 };
        bool reported{ false };
    };
}
