#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"

namespace foc
{
    // Teknic M-2310P-LN-04K, the motor of the TI 2MTR-DYNO and NXP FRDM-MC kits (40 V bus, 6000 rpm,
    // 0.274 N·m). Per-phase R and L from the 0.72 Ω / 0.40 mH line-to-line datasheet values, flux from
    // 4.64 Vpk/krpm line-to-line, J from 0.001 oz·in·s². B is not published and is assumed.
    // Sources and derivation: documentation/theory/foc-plant-models.md, "Reference motors".
    struct M_2310P_LN_04K
    {
        static inline const foc::Volts ratedSupply{ 40.0f };

        static inline const ThreePhaseMotorModel::Parameters parameters{
            .R = foc::Ohm{ 0.36f },
            .Ld = foc::Henry{ 0.0002f },
            .Lq = foc::Henry{ 0.0002f },
            .psi_f = foc::Weber{ 0.0064f },
            .p = 4,
            .J = foc::KilogramMeterSquared{ 0.00000706f },
            .B = foc::NewtonMeterSecondPerRadian{ 0.000015f },
            .maxSupportedCurrent = foc::Ampere{ 20.0f },
        };
    };
}
