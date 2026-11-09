#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/Math.hpp"
#include <cmath>
#include <optional>

namespace foc
{
    // Copper's resistance rises 0.393 % per kelvin.
    inline constexpr float copperTemperatureCoefficient{ 0.00393f };

    inline std::optional<Celsius> WindingTemperatureFromResistance(Ohm online, Ohm calibrated, Celsius calibration)
    {
        if (!(calibrated.Value() > 0.0f) || !(online.Value() > 0.0f) || !math::IsFinite(online.Value()))
            return std::nullopt;

        return Celsius{ calibration.Value() + (online.Value() / calibrated.Value() - 1.0f) / copperTemperatureCoefficient };
    }
}
