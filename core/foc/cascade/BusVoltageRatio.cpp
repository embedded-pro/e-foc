#include "core/foc/cascade/BusVoltageRatio.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <algorithm>

namespace foc
{
    namespace
    {
        constexpr float lowestRatio{ 0.5f };
        constexpr float highestRatio{ 2.0f };
    }

    void BusVoltageRatio::Configure(Volts configuredVoltage)
    {
        configured = configuredVoltage.Value();
        ratio = 1.0f;
        inverse = 1.0f;
    }

    void BusVoltageRatio::Track(Volts measured)
    {
        if (configured <= 0.0f || !IsFiniteValue(measured.Value()))
            return;

        const auto tracked = std::clamp(measured.Value() / configured, lowestRatio, highestRatio);
        ratio = tracked;
        inverse = 1.0f / tracked;
    }
}
