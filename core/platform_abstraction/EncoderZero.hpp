#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace application
{
    class EncoderZero
    {
    public:
        OPTIMIZE_FOR_SPEED foc::Radians AngleOf(foc::Radians reading) const
        {
            constexpr float pi = std::numbers::pi_v<float>;
            constexpr float twoPi = 2.0f * pi;

            auto angle = reading.Value() - zero;
            if (angle >= pi)
                angle -= twoPi;
            else if (angle < -pi)
                angle += twoPi;

            return foc::Radians{ angle };
        }

        void SetZero(foc::Radians reading)
        {
            Set(reading, foc::Radians{ 0.0f });
        }

        void Set(foc::Radians reading, foc::Radians angle)
        {
            zero = foc::detail::PositionWithWrapAround(reading.Value() - angle.Value());
        }

    private:
        float zero{ 0.0f };
    };
}
