#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace foc
{
    struct CurrentStreamSample
    {
        uint8_t sequence{ 0 };
        Ampere meanId{ 0.0f };
        Ampere meanIq{ 0.0f };
        Radians position{ 0.0f };
    };

    // The control interrupt accumulates and publishes; the event loop configures and takes.
    class CurrentStream
    {
    public:
        static constexpr uint8_t capacity{ 16 };

        void Start(uint32_t samplesPerWindow)
        {
            Request(samplesPerWindow);
        }

        void Stop()
        {
            Request(0);
        }

        void Restart()
        {
            requested = requested + 1;
        }

        std::optional<CurrentStreamSample> Take()
        {
            const uint8_t oldest = tail;
            if (oldest == head)
                return std::nullopt;

            std::atomic_signal_fence(std::memory_order_seq_cst);
            const auto sample = slots[oldest % capacity];
            std::atomic_signal_fence(std::memory_order_seq_cst);
            tail = static_cast<uint8_t>(oldest + 1);
            return sample;
        }

        ALWAYS_INLINE_HOT void Accumulate(float id, float iq, float position)
        {
            if (requested != applied)
                Reopen();

            if (length == 0)
                return;

            idSum += id;
            iqSum += iq;

            if (++count == length)
                Publish(position);
        }

    private:
        void Request(uint32_t samplesPerWindow)
        {
            requestedLength = samplesPerWindow;
            tail = head;
            requested = requested + 1;
        }

        ALWAYS_INLINE_HOT void Reopen()
        {
            applied = requested;
            length = requestedLength;
            inverseLength = length != 0 ? 1.0f / static_cast<float>(length) : 0.0f;
            count = 0;
            idSum = 0.0f;
            iqSum = 0.0f;
        }

        ALWAYS_INLINE_HOT void Publish(float position)
        {
            const uint8_t next = head;
            if (static_cast<uint8_t>(next - tail) != capacity)
            {
                slots[next % capacity] = CurrentStreamSample{ sequence, Ampere{ idSum * inverseLength }, Ampere{ iqSum * inverseLength }, Radians{ position } };
                std::atomic_signal_fence(std::memory_order_seq_cst);
                head = static_cast<uint8_t>(next + 1);
            }

            sequence = static_cast<uint8_t>(sequence + 1);
            count = 0;
            idSum = 0.0f;
            iqSum = 0.0f;
        }

        volatile uint32_t requestedLength{ 0 };
        volatile uint32_t requested{ 0 };
        volatile uint8_t head{ 0 };
        volatile uint8_t tail{ 0 };

        uint32_t applied{ 0 };
        uint32_t length{ 0 };
        float inverseLength{ 0.0f };
        uint32_t count{ 0 };
        float idSum{ 0.0f };
        float iqSum{ 0.0f };
        uint8_t sequence{ 0 };
        std::array<CurrentStreamSample, capacity> slots{};
    };
}
