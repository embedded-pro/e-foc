#pragma once

#include "tools/simulator/model/Model.hpp"

namespace simulator
{
    struct JK42BLS01_X038ED
    {
        static inline const ThreePhaseMotorModel::Parameters parameters{
            .R = foc::Ohm{ 0.073f },
            .Ld = foc::Henry{ 0.0005f },
            .Lq = foc::Henry{ 0.0005f },
            .psi_f = foc::Weber{ 0.007f },
            .p = 4,
            .J = foc::KilogramMeterSquared{ 0.0000075f },
            .B = foc::NewtonMeterSecondPerRadian{ 0.00002f },
        };
    };
}
