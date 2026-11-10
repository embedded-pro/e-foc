#include "tools/virtual_motor/platform/ProfileRunner.hpp"
#include <algorithm>

namespace virtual_motor
{
    ProfileRunner::ProfileRunner(PlantDisturbances& disturbances)
        : disturbances(disturbances)
    {}

    void ProfileRunner::Report(InstrumentReports& reportsTo)
    {
        reports = &reportsTo;
    }

    std::optional<port::RefusalReason> ProfileRunner::Run(const port::ProfileRequest& profile)
    {
        if (auto refusal = Check(profile))
            return refusal;

        steps = profile.steps;
        next = 0;
        elapsed = 0;
        watch.Arm(profile.trigger);
        armed = true;
        running = false;
        State(port::ProfileStateKind::armed);
        return std::nullopt;
    }

    void ProfileRunner::Stop()
    {
        armed = false;
        running = false;
        disturbances.ClearAll();
        State(port::ProfileStateKind::stopped);
    }

    void ProfileRunner::Abandon()
    {
        if (!Active())
            return;

        armed = false;
        running = false;
        State(port::ProfileStateKind::stopped);
    }

    bool ProfileRunner::Active() const
    {
        return armed || running;
    }

    void ProfileRunner::Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection)
    {
        bool changed{ false };
        if (armed && watch.Fires(sample, leds, protection))
        {
            armed = false;
            running = true;
            start = period;
            changed = true;
        }

        if (!running)
            return;

        elapsed = static_cast<uint32_t>(period - start);
        changed = ApplyDue() || changed;
        running = next != steps.size();

        if (!running)
            State(port::ProfileStateKind::finished);
        else if (changed)
            State(port::ProfileStateKind::running);
    }

    std::optional<port::RefusalReason> ProfileRunner::Check(const port::ProfileRequest& profile) const
    {
        if (!profile.readable || profile.steps.size() > port::maxProfileSteps)
            return port::RefusalReason::outOfRange;
        if (!std::ranges::is_sorted(profile.steps, {}, &port::ProfileStep::at))
            return port::RefusalReason::outOfRange;
        if (auto refusal = TriggerWatch::Check(profile.trigger))
            return refusal;

        for (const auto& step : profile.steps)
            if (auto refusal = disturbances.Check(step))
                return refusal;
        return std::nullopt;
    }

    bool ProfileRunner::ApplyDue()
    {
        const auto first = next;
        while (next != steps.size() && steps[next].at <= elapsed)
            disturbances.Apply(steps[next++]);
        return next != first;
    }

    void ProfileRunner::State(port::ProfileStateKind state)
    {
        if (reports != nullptr)
            reports->ReportProfileState(port::ProfileState{ state, static_cast<uint16_t>(next), static_cast<uint16_t>(steps.size()), elapsed });
    }
}
