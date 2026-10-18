#pragma once

#include "infra/util/Function.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <utility>

namespace foc
{
    template<typename Signature>
    class IsrCallback;

    // A callback that the thread replaces while an interrupt keeps invoking it. An infra::Function assignment is several
    // instructions, so an interrupt landing in the middle of one runs a half-written function. Here the thread writes the slot
    // the interrupt is not using and publishes it with one atomic store of the index; the interrupt reads the index once and
    // runs a complete function, the old one or the new one.
    //
    // That is safe on a single core because the invoker preempts the setter: whenever the setter runs, the invoker is not part
    // way through a call, so the slot it is not pointing at is idle. It requires that Set and Clear are called from a context the
    // invoker preempts (the thread, or a lower-priority interrupt), and that a callback replaces itself at most once per call.
    template<typename... Args>
    class IsrCallback<void(Args...)>
    {
    public:
        using Callback = infra::Function<void(Args...)>;

        void Set(const Callback& callback)
        {
            const uint8_t next = lastSet == 0 ? 1 : 0;

            slots[next] = callback;
            lastSet = next;
            active.store(next, std::memory_order_release);
        }

        void Clear()
        {
            active.store(none, std::memory_order_release);
        }

        bool IsSet() const
        {
            return active.load(std::memory_order_acquire) != none;
        }

        void operator()(Args... args)
        {
            const auto index = active.load(std::memory_order_acquire);

            if (index != none)
                slots[index](std::forward<Args>(args)...);
        }

    private:
        static constexpr uint8_t none{ 2 };

        std::array<Callback, 2> slots;
        std::atomic<uint8_t> active{ none };
        uint8_t lastSet{ 1 };
    };
}
