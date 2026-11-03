#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>

namespace application
{
    struct BoardProtectionLimits
    {
        foc::Volts overVoltage;
        foc::Volts underVoltage;
    };

    ALWAYS_INLINE_HOT bool ExceedsOverCurrent(foc::Ampere a, foc::Ampere b, foc::Ampere c, foc::Ampere limit)
    {
        const auto bound = limit.Value();
        return std::abs(a.Value()) > bound || std::abs(b.Value()) > bound || std::abs(c.Value()) > bound;
    }

    // Event-loop context only: readBusVoltage may block, and it is skipped while a latch already decides the answer.
    template<typename ReadBusVoltage>
    PlatformFactory::BoardProtectionState EvaluateBoardProtection(bool overCurrentLatched, bool pwmFaultLatched, const BoardProtectionLimits& limits, ReadBusVoltage&& readBusVoltage)
    {
        if (overCurrentLatched || pwmFaultLatched)
            return PlatformFactory::BoardProtectionState::asserted;

        const auto bus = readBusVoltage().Value();
        const auto inRange = bus >= limits.underVoltage.Value() && bus <= limits.overVoltage.Value();

        return inRange ? PlatformFactory::BoardProtectionState::clear : PlatformFactory::BoardProtectionState::asserted;
    }
}
