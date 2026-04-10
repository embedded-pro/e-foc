#pragma once

#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include <gmock/gmock.h>

namespace services
{
    class MechanicalParametersIdentificationMock
        : public MechanicalParametersIdentification
    {
    public:
        MOCK_METHOD(void, EstimateFrictionAndInertia,
            (const foc::NewtonMeter& torqueConstant,
                std::size_t numberOfPolePairs,
                const Config& config,
                const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
                    std::optional<foc::NewtonMeterSecondSquared>)>& onDone),
            (override));
    };
}
