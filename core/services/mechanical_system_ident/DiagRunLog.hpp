#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace diag
{
    // Diagnostic only: the runs the online estimator's constant-speed friction fit sees, by the same 3 % speed band,
    // with their raw mean torque, mean acceleration, the inertia subtracted and the observation they started at,
    // read with estimate_status. Not for merging.
    struct Run
    {
        uint32_t start{ 0 };
        uint32_t observations{ 0 };
        float speed{ 0.0f };
        float torque{ 0.0f };
        float acceleration{ 0.0f };
        float inertia{ 0.0f };
    };

    class RunLog
    {
    public:
        static constexpr std::size_t capacity{ 32 };
        static constexpr float band{ 0.03f };
        static constexpr uint32_t minimumObservations{ 16 };
        static constexpr uint32_t maximumObservations{ 256 };

        void Add(float speed, float torque, float acceleration, float inertia)
        {
            ++observation;
            if (count != 0 && std::abs(speed - speedSum / static_cast<float>(count)) > band * std::abs(speedSum / static_cast<float>(count)))
                End();

            if (count == 0)
                start = observation;

            speedSum += speed;
            torqueSum += torque;
            accelerationSum += acceleration;
            lastInertia = inertia;

            if (++count == maximumObservations)
                End();
        }

        void Skip()
        {
            ++observation;
            End();
        }

        std::size_t Count() const
        {
            return logged < capacity ? logged : capacity;
        }

        const Run& Entry(std::size_t index) const
        {
            const auto first = logged < capacity ? 0 : logged - capacity;
            return entries[(first + index) % capacity];
        }

        std::array<float, 2> friction{ -1.0f, -1.0f };

    private:
        void End()
        {
            if (count >= minimumObservations)
            {
                const auto n = static_cast<float>(count);
                entries[logged % capacity] = Run{ start, count, speedSum / n, torqueSum / n, accelerationSum / n, lastInertia };
                ++logged;
            }

            count = 0;
            speedSum = 0.0f;
            torqueSum = 0.0f;
            accelerationSum = 0.0f;
        }

        std::array<Run, capacity> entries{};
        std::size_t logged{ 0 };
        uint32_t observation{ 0 };
        uint32_t start{ 0 };
        uint32_t count{ 0 };
        float speedSum{ 0.0f };
        float torqueSum{ 0.0f };
        float accelerationSum{ 0.0f };
        float lastInertia{ 0.0f };
    };

    inline RunLog runLog;
}
