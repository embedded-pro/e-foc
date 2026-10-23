#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace diag
{
    // Diagnostic only: the mean speed, torque and acceleration of each run of constant-speed observations the
    // online mechanical estimator was given, read with estimate_status. Not for merging.
    struct Plateau
    {
        float speed{ 0.0f };
        float torque{ 0.0f };
        float acceleration{ 0.0f };
        uint32_t observations{ 0 };
    };

    class PlateauLog
    {
    public:
        static constexpr std::size_t capacity{ 24 };
        static constexpr uint32_t minimumObservations{ 8 };

        void Add(float speed, float torque, float acceleration)
        {
            if (runObservations != 0 && (speed > 0.0f) != (runSpeed > 0.0f))
                End();

            runSpeed += speed;
            runTorque += torque;
            runAcceleration += acceleration;
            ++runObservations;
        }

        void End()
        {
            if (runObservations >= minimumObservations)
            {
                const auto n = static_cast<float>(runObservations);
                entries[logged % capacity] = Plateau{ runSpeed / n, runTorque / n, runAcceleration / n, runObservations };
                ++logged;
            }

            runSpeed = 0.0f;
            runTorque = 0.0f;
            runAcceleration = 0.0f;
            runObservations = 0;
        }

        std::size_t Count() const
        {
            return logged < capacity ? logged : capacity;
        }

        const Plateau& Entry(std::size_t index) const
        {
            const auto first = logged < capacity ? 0 : logged - capacity;
            return entries[(first + index) % capacity];
        }

    private:
        std::array<Plateau, capacity> entries{};
        std::size_t logged{ 0 };
        float runSpeed{ 0.0f };
        float runTorque{ 0.0f };
        float runAcceleration{ 0.0f };
        uint32_t runObservations{ 0 };
    };

    inline PlateauLog plateauLog;
}
