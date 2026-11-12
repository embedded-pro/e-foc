#include "core/supervision/SustainedCondition.hpp"

namespace supervision
{
    bool SustainedCondition::Held(bool condition, infra::TimePoint now, infra::Duration required)
    {
        if (!condition)
        {
            since.reset();
            return false;
        }

        if (!since.has_value())
            since = now;

        return now - *since >= required;
    }

    bool SustainedCondition::HeldCount(bool condition, uint32_t required)
    {
        if (!condition)
        {
            count = 0;
            return false;
        }

        if (count < required)
            ++count;

        return count >= required;
    }

    bool SustainedCondition::ReportOnce(bool trigger)
    {
        if (reported || !trigger)
            return false;

        reported = true;
        return true;
    }

    bool SustainedCondition::Reported() const
    {
        return reported;
    }

    void SustainedCondition::Reset()
    {
        since.reset();
        count = 0;
        reported = false;
    }
}
