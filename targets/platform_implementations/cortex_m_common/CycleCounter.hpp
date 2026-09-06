#pragma once

#include <cstdint>

namespace application
{
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
