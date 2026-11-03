#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>

namespace application
{
    struct BoardProtectionLimits
    {
        float overVoltageVolts;
        float underVoltageVolts;
    };

    ALWAYS_INLINE_HOT bool ExceedsOverCurrent(foc::Ampere a, foc::Ampere b, foc::Ampere c, float limit)
    {
        return std::abs(a.Value()) > limit || std::abs(b.Value()) > limit || std::abs(c.Value()) > limit;
    }

    // Event-loop context only: readBusVoltage may block, and it is skipped while a latch already decides the answer.
    template<typename ReadBusVoltage>
    PlatformFactory::BoardProtectionState EvaluateBoardProtection(bool overCurrentLatched, bool pwmFaultLatched, const BoardProtectionLimits& limits, ReadBusVoltage&& readBusVoltage)
    {
        if (overCurrentLatched || pwmFaultLatched)
            return PlatformFactory::BoardProtectionState::asserted;

        const auto bus = readBusVoltage().Value();
        const auto inRange = bus >= limits.underVoltageVolts && bus <= limits.overVoltageVolts;

        return inRange ? PlatformFactory::BoardProtectionState::clear : PlatformFactory::BoardProtectionState::asserted;
    }
}
