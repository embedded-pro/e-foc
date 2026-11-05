#include "core/state_machine/CurrentOffsetRefresh.hpp"
#include <cmath>

namespace application
{
    CurrentOffsetRefresh::CurrentOffsetRefresh(Offsets offsets, Standstill standstill, services::Tracer& tracer)
        : offsets(offsets)
        , standstill(standstill)
        , tracer(tracer)
    {}

    bool CurrentOffsetRefresh::IsDue() const
    {
        return offsets.has_value() && !fresh;
    }

    void CurrentOffsetRefresh::MarkStale()
    {
        fresh = false;
    }

    void CurrentOffsetRefresh::Start(const foc::PhaseCurrents& storedOffsets, const infra::Function<void(state_machine::OffsetRefreshOutcome)>& onRefreshed)
    {
        stored = storedOffsets;
        onDone = onRefreshed;
        ++run;

        if (!standstill)
        {
            Measure();
            return;
        }

        tracer.Trace() << "[SM] Waiting for the rotor to come to rest before measuring the current offsets";
        const auto token = run;
        standstill->get().Await([this, token](bool atRest)
            {
                if (token != run || !onDone)
                    return;

                if (atRest)
                    Measure();
                else
                {
                    tracer.Trace() << "[SM] Enable refused: the rotor did not come to rest";
                    onDone(state_machine::OffsetRefreshOutcome::failed);
                }
            });
    }

    bool CurrentOffsetRefresh::IsRunning() const
    {
        return static_cast<bool>(onDone);
    }

    void CurrentOffsetRefresh::Abort()
    {
        if (!onDone)
            return;

        ++run;
        onDone = nullptr;
        if (standstill)
            standstill->get().Abort();
        offsets->get().Abort();
    }

    void CurrentOffsetRefresh::Measure()
    {
        tracer.Trace() << "[SM] Measuring current sensor offsets before enabling";
        const auto token = run;
        offsets->get().Measure([this, token](std::optional<foc::PhaseCurrents> measured)
            {
                if (token == run && onDone)
                    Conclude(measured);
            });
    }

    void CurrentOffsetRefresh::Conclude(const std::optional<foc::PhaseCurrents>& measured)
    {
        if (!measured.has_value())
        {
            tracer.Trace() << "[SM] Enable refused: the current offsets could not be measured";
            onDone(state_machine::OffsetRefreshOutcome::failed);
        }
        else if (!IsNearStored(*measured))
        {
            tracer.Trace() << "[SM] Enable refused: a current offset is more than 50 mA from the calibration; recalibrate";
            onDone(state_machine::OffsetRefreshOutcome::drifted);
        }
        else
        {
            offsets->get().Apply(*measured);
            fresh = true;
            onDone(state_machine::OffsetRefreshOutcome::refreshed);
        }
    }

    bool CurrentOffsetRefresh::IsNearStored(const foc::PhaseCurrents& measured) const
    {
        return std::abs(measured.a.Value() - stored.a.Value()) <= driftLimit.Value() &&
               std::abs(measured.b.Value() - stored.b.Value()) <= driftLimit.Value() &&
               std::abs(measured.c.Value() - stored.c.Value()) <= driftLimit.Value();
    }
}
