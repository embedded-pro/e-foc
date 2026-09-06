#pragma once

#include <cstdint>

namespace application
{
    // A free-running read of the Cortex-M cycle counter, for measuring how long the control
    // interrupt takes. hal::cortex::DataWatchpointAndTrace exposes the same counter as a
    // start/stop stopwatch, which cannot serve two callers; this leaves it running and takes
    // deltas, so any number of callers can measure independently.
    //
    // Deltas are wrap-safe: the counter is 32 bits and unsigned subtraction is modular, so a run
    // that straddles the wrap still yields its true length. At 120 MHz the counter wraps about
    // every 36 seconds, which no single interrupt approaches.
    class CycleCounter
    {
    public:
        CycleCounter()
        {
            Register(coreDebugDemcr) |= demcrTrcena;
            Register(dwtCtrl) |= dwtCyccntena;
        }

        static uint32_t Now()
        {
            return Register(dwtCyccnt);
        }

    private:
        static constexpr uintptr_t coreDebugDemcr = 0xE000EDFCu;
        static constexpr uint32_t demcrTrcena = 1u << 24;
        static constexpr uintptr_t dwtCtrl = 0xE0001000u;
        static constexpr uint32_t dwtCyccntena = 1u << 0;
        static constexpr uintptr_t dwtCyccnt = 0xE0001004u;

        static volatile uint32_t& Register(uintptr_t address)
        {
            return *reinterpret_cast<volatile uint32_t*>(address);
        }
    };
}
