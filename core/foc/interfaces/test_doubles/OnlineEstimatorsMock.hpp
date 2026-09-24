#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include <gmock/gmock.h>

namespace foc
{
    class OnlineMechanicalEstimatorMock
        : public OnlineMechanicalEstimator
    {
    public:
        MOCK_METHOD(void, SetTorqueConstant, (NewtonMeter kt), (override));
        MOCK_METHOD(void, SetInitialEstimate, (NewtonMeterSecondSquared inertia, NewtonMeterSecondPerRadian friction), (override));
        MOCK_METHOD(void, Update, (const MechanicalWindow& window), (override));
        MOCK_METHOD(NewtonMeterSecondSquared, CurrentInertia, (), (const, override));
        MOCK_METHOD(NewtonMeterSecondPerRadian, CurrentFriction, (), (const, override));
    };

    class OnlineElectricalEstimatorMock
        : public OnlineElectricalEstimator
    {
    public:
        MOCK_METHOD(void, SetInitialEstimate, (Ohm resistance, MilliHenry inductance), (override));
        MOCK_METHOD(void, Update, (const ElectricalWindow& window), (override));
        MOCK_METHOD(Ohm, CurrentResistance, (), (const, override));
        MOCK_METHOD(MilliHenry, CurrentInductance, (), (const, override));
    };
}
