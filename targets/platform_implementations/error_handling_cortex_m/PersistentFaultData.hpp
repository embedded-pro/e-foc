#pragma once

#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include <array>
#include <cstdint>

namespace application
{
    struct PersistentFaultData
    {
        static constexpr uint32_t kMagicValid = 0xDEADBEEFu;
        static constexpr std::size_t kMaxStackTraceEntries = 243u;

        uint32_t magic{ 0 };
        uint32_t r0{ 0 };
        uint32_t r1{ 0 };
        uint32_t r2{ 0 };
        uint32_t r3{ 0 };
        uint32_t r12{ 0 };
        uint32_t lr{ 0 };
        uint32_t pc{ 0 };
        uint32_t psr{ 0 };
        uint32_t cfsr{ 0 };
        uint32_t mmfar{ 0 };
        uint32_t bfar{ 0 };
        uint32_t stackTraceCount{ 0 };
        std::array<uint32_t, kMaxStackTraceEntries> stackTrace{};

        bool IsValid() const;
        void Invalidate();
    };

    static_assert(sizeof(PersistentFaultData) == 1024u,
        "PersistentFaultData must be exactly 1024 bytes");

    void FormatFaultData(const PersistentFaultData& data, infra::BoundedString& out);

    extern PersistentFaultData persistentFaultData; // NOSONAR — placed in .noinit section by linker; must be global for HardFault ISR access
}
