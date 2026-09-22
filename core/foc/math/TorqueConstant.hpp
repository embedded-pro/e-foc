#pragma once

#include "core/foc/interfaces/Units.hpp"
#include <cstddef>

namespace foc
{
    // Sinusoidal FOC with the amplitude-invariant Clarke transform: T = 3/2 · p · ψf · iq, the same
    // factor the plant model applies, so an identified ψf and p fix Kt without a separate constant.
    inline NewtonMeter TorqueConstantFor(std::size_t polePairs, Weber fluxLinkage)
    {
        return NewtonMeter{ 1.5f * static_cast<float>(polePairs) * fluxLinkage.Value() };
    }
}
