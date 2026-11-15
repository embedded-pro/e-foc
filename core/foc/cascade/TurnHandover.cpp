#include "core/foc/cascade/TurnHandover.hpp"

namespace foc
{
    void TurnHandover::Track(float angle)
    {
        if (!held)
            counter.Track(angle);
    }

    void TurnHandover::Restart()
    {
        counter.Restart();
        restartedWhileHeld = held;
    }

    std::optional<TurnCount> TurnHandover::Hold()
    {
        held = true;
        restartedWhileHeld = false;

        if (!counter.IsPrimed())
        {
            published.Clear();
            return std::nullopt;
        }

        published.Write(counter.Count());
        return counter.Count();
    }

    void TurnHandover::Release()
    {
        if (!held)
            return;

        held = false;
        const auto last = published.Read();
        if (!restartedWhileHeld && last)
            counter.Seed(*last);
    }

    Radians TurnHandover::Position() const
    {
        if (!held)
            return Radians{ PositionOf(counter.Count()) };

        return Radians{ PositionOf(published.Read().value_or(counter.Count())) };
    }
}
