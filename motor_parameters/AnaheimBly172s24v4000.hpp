#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"

namespace foc
{
    // Anaheim Automation BLY172S-24V-4000, a 42 mm 24 V BLDC (4000 rpm, 53 W, 0.127 N·m). Per-phase R, L
    // and flux from TI's InstaSPIN identification of this motor (user.h), consistent with the BLY17
    // datasheet's 1.20 mH line-to-line and 3.35 V/krpm; J from 0.00068 oz·in·s². B is not published and
    // is assumed. Sources and derivation: documentation/theory/foc-plant-models.md, "Reference motors".
    struct BLY172S_24V_4000
    {
        static inline const foc::Volts ratedSupply{ 24.0f };

        static inline const ThreePhaseMotorModel::Parameters parameters{
            .R = foc::Ohm{ 0.405f },
            .Ld = foc::Henry{ 0.00064f },
            .Lq = foc::Henry{ 0.00064f },
            .psi_f = foc::Weber{ 0.00544f },
            .p = 4,
            .J = foc::KilogramMeterSquared{ 0.0000048f },
            .B = foc::NewtonMeterSecondPerRadian{ 0.00001f },
            .maxSupportedCurrent = foc::Ampere{ 10.0f },
        };
    };
}
