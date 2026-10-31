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
        using Weber = infra::BaseUnit<13>;
        using KilogramMeterSquared = infra::BaseUnit<14>;

        using Radians = Angle;
        using RevPerSecond = Revolution::Div<infra::Second>;
        using RevPerMinute = Revolution::Div<infra::Minute>;
    }

    using Ampere = infra::Quantity<infra::Ampere, float>;
    using Volts = infra::Quantity<infra::Volt, float>;
    using Radians = infra::Quantity<unit::Radians, float>;
    using RadiansPerSecond = infra::Quantity<unit::Radians::Div<infra::Second>, float>;
    using RadiansPerSecondSquared = infra::Quantity<unit::Radians::Div<infra::Second>::Div<infra::Second>, float>;
    using Nm = infra::Quantity<unit::NewtonMeter, float>;
    using NewtonMeter = Nm;
    using NewtonMeterSecondSquared = infra::Quantity<unit::NewtonMeter::Mul<infra::Second>::Mul<infra::Second>, float>;
    using NewtonMeterSecondPerRadian = infra::Quantity<unit::NewtonMeter::Mul<infra::Second>::Div<unit::Radians>, float>;
    using RevPerMinute = infra::Quantity<unit::RevPerMinute, float>;
    using Henry = infra::Quantity<unit::Henry, float>;
    using Weber = infra::Quantity<unit::Weber, float>;
    using KilogramMeterSquared = infra::Quantity<unit::KilogramMeterSquared, float>;
    using MilliHenry = infra::Quantity<unit::Henry::Scale<infra::StaticRational<1, 1000>>, float>;
    using Ohm = infra::Quantity<unit::Ohm, float>;
}
