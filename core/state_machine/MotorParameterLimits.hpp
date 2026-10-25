#pragma once

#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include <cmath>

namespace state_machine
{
    inline bool IsAcceptableMotorParameter(services::MotorParameter parameter, float value)
    {
        using foc::CommandLimits;

        switch (parameter)
        {
            case services::MotorParameter::resistance:
                return foc::IsWithinInclusive(value, CommandLimits::minResistance, CommandLimits::maxResistance);
            case services::MotorParameter::inductance:
                return foc::IsWithinInclusive(value, CommandLimits::minInductanceMilliHenry, CommandLimits::maxInductanceMilliHenry);
            case services::MotorParameter::polePairs:
                return foc::IsWithinInclusive(value, 1.0f, CommandLimits::maxPolePairs) && std::round(value) == value;
            case services::MotorParameter::fluxLinkage:
                return value == 0.0f || foc::IsWithinInclusive(value, CommandLimits::minFluxLinkage, CommandLimits::maxFluxLinkage);
            case services::MotorParameter::inertia:
                return foc::IsWithinInclusive(value, CommandLimits::minInertia, CommandLimits::maxInertia);
            default:
                return foc::IsWithinInclusive(value, 0.0f, CommandLimits::maxViscousFriction);
        }
    }
}
