#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <bit>
#include <cstdint>

namespace foc
{
    ALWAYS_INLINE_HOT bool IsFiniteValue(float value)
    {
        constexpr uint32_t exponentMask = 0x7F800000u;

        return (std::bit_cast<uint32_t>(value) & exponentMask) != exponentMask;
    }
}
