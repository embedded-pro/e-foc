#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace services
{
    struct IdentificationObservation
    {
        float acceleration{ 0.0f };
        float speed{ 0.0f };
        float torque{ 0.0f };
        uint32_t sample{ 0 };
        uint32_t windowSamples{ 1 };
    };

    // Single producer (sampling interrupt) posts, single consumer (event loop) takes.
    class ObservationQueue
    {
    public:
        static constexpr uint32_t capacity{ 16 };

        enum class PostResult : uint8_t
        {
            posted,
            halfFull,
            full
        };

        void Clear();
        PostResult Post(const IdentificationObservation& observation);
        std::optional<IdentificationObservation> Take();

    private:
        std::array<IdentificationObservation, capacity> observations{};
        std::atomic<uint32_t> posted{ 0 };
        std::atomic<uint32_t> taken{ 0 };
    };
}
