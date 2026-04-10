#pragma once

#include "numerical/math/QNumber.hpp"

namespace foc
{
    template<typename T>
    T CreateNormalizedAngle(T angle)
    {
        static const T two_pi = T(2.0f * static_cast<float>(M_PI));

        really_assert(angle >= -two_pi && angle < two_pi);

        return T(angle / two_pi);
    }
}
