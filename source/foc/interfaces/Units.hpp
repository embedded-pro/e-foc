#pragma once

#include "infra/util/Unit.hpp"

namespace foc
{
    namespace unit
    {
        using Revolution = infra::BaseUnit<8>;
        using NewtonMeter = infra::BaseUnit<9>;
        using Angle = infra::BaseUnit<10>;
        using Henry = infra::BaseUnit<11>;
        using Ohm = infra::BaseUnit<12>;

        using Radians = Angle::Scale<infra::StaticRational<10, 0>>;
        using RevPerSecond = Revolution::Div<infra::Second>::Inverse;
        using RevPerMinute = RevPerSecond::Scale<infra::StaticRational<60, 1>>;
    }

    using Ampere = infra::Quantity<infra::Ampere, float>;
    using Volts = infra::Quantity<infra::Volt, float>;
    using Radians = infra::Quantity<unit::Radians, float>;
    using RadiansPerSecond = infra::Quantity<unit::Radians::Div<infra::Second>, float>;
    using HallState = uint8_t;
    using Nm = infra::Quantity<unit::NewtonMeter, float>;
    using NewtonMeter = Nm;
    using NewtonMeterSecondSquared = infra::Quantity<unit::NewtonMeter::Mul<infra::Second>::Mul<infra::Second>, float>;
    using NewtonMeterSecondPerRadian = infra::Quantity<unit::NewtonMeter::Mul<infra::Second>::Div<unit::Radians>, float>;
    using RevPerMinute = infra::Quantity<unit::RevPerMinute, float>;
    using Henry = infra::Quantity<unit::Henry, float>;
    using MilliHenry = infra::Quantity<unit::Henry::Scale<infra::StaticRational<1, 1000>>, float>;
    using Ohm = infra::Quantity<unit::Ohm, float>;
}
