#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <array>
#include <atomic>
#include <cstdint>

namespace foc
{
    namespace detail
    {
        constexpr uint8_t NextFreeSlot(uint8_t slotCount, uint8_t ready, uint8_t held)
        {
            if (ready != held)
                return static_cast<uint8_t>(slotCount * (slotCount - 1u) / 2u - ready - held);

            const auto afterReady = ready == slotCount - 1u ? 0u : ready + 1u;

            return static_cast<uint8_t>(afterReady);
        }
    }

    // Lock-free single-writer/single-reader handoff across the ISR boundary: Publish() never
    // writes into the slot the reader currently holds, so Acquire() always returns a whole value.
    template<typename T>
    class TripleBuffer
    {
    public:
        ALWAYS_INLINE_HOT void Publish(const T& value)
        {
            slots[writeSlot] = value;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            ready = writeSlot;
            writeSlot = NextFreeSlot();
        }

        ALWAYS_INLINE_HOT const T& Acquire()
        {
            held = ready;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            return slots[held];
        }

    private:
        ALWAYS_INLINE_HOT uint8_t NextFreeSlot() const
        {
            return detail::NextFreeSlot(slotCount, ready, held);
        }

        static constexpr uint8_t slotCount = 3;

        std::array<T, slotCount> slots{};
        uint8_t writeSlot{ 1 };
        volatile uint8_t ready{ 0 };
        volatile uint8_t held{ 0 };
    };
}
