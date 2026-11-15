#pragma once

#include "core/foc/math/TurnCounter.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <optional>

namespace foc
{
    // Counts turns on the event loop until the position cascade holds the count; the cascade then publishes its own from the outer-loop interrupt.
    class TurnHandover
    {
    public:
        void Track(float angle);
        void Restart();
        std::optional<TurnCount> Hold();
        void Release();
        Radians Position() const;

        OPTIMIZE_FOR_SPEED void Publish(const TurnCount& count)
        {
            published.Write(count);
        }

    private:
        TurnCounter counter;
        bool held{ false };
        bool restartedWhileHeld{ false };
        // Written by the outer-loop interrupt while held.
        PublishedTurnCount published;
    };
}
