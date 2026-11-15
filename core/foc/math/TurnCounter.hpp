#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>
#include <numbers>
#include <optional>

namespace foc
{
    struct TurnCount
    {
        int32_t turns{ 0 };
        // Mechanical angle within the turn, [−π, π), of the sample the turns were counted at.
        float angle{ 0.0f };
    };

    ALWAYS_INLINE_HOT float PositionOf(const TurnCount& count)
    {
        return static_cast<float>(count.turns) * 2.0f * std::numbers::pi_v<float> + count.angle;
    }

    // Subtracts the turns before the angles, so a count far from the zero keeps the angle's resolution.
    ALWAYS_INLINE_HOT float Difference(const TurnCount& to, const TurnCount& from)
    {
        return static_cast<float>(to.turns - from.turns) * 2.0f * std::numbers::pi_v<float> + (to.angle - from.angle);
    }

    class TurnCounter
    {
    public:
        void Restart()
        {
            count = TurnCount{};
            primed = false;
        }

        void Seed(const TurnCount& seed)
        {
            count = seed;
            primed = true;
        }

        OPTIMIZE_FOR_SPEED void Track(float angle)
        {
            constexpr float pi = std::numbers::pi_v<float>;

            if (primed)
            {
                const float step = angle - count.angle;
                if (step < -pi)
                    ++count.turns;
                else if (step > pi)
                    --count.turns;
            }

            count.angle = angle;
            primed = true;
        }

        bool IsPrimed() const
        {
            return primed;
        }

        const TurnCount& Count() const
        {
            return count;
        }

    private:
        TurnCount count;
        bool primed{ false };
    };

    // Written by one context and read by a lower-priority one, which reads again when the writer pre-empted it.
    class PublishedTurnCount
    {
    public:
        OPTIMIZE_FOR_SPEED void Write(const TurnCount& count)
        {
            sequence = sequence + 1;
            turns = count.turns;
            angle = count.angle;
            valid = true;
            sequence = sequence + 1;
        }

        void Clear()
        {
            sequence = sequence + 1;
            valid = false;
            sequence = sequence + 1;
        }

        std::optional<TurnCount> Read() const
        {
            uint32_t before{};
            TurnCount count{};
            bool written{};
            do
            {
                before = sequence;
                count = TurnCount{ turns, angle };
                written = valid;
            } while ((before & 1u) != 0 || before != sequence);

            if (!written)
                return std::nullopt;

            return count;
        }

    private:
        volatile uint32_t sequence{ 0 };
        volatile int32_t turns{ 0 };
        volatile float angle{ 0.0f };
        volatile bool valid{ false };
    };
}
