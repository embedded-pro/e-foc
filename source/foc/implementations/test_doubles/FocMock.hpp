#pragma once

#include "source/foc/interfaces/Foc.hpp"
#include <gmock/gmock.h>

namespace foc
{
    class FocTorqueMock
        : public FocTorque
    {
    public:
        MOCK_METHOD(void, SetPolePairs, (std::size_t polePairs), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (IdAndIqPoint), (override));
        MOCK_METHOD(void, SetCurrentTunings, (Volts Vdc, const IdAndIqTunings& tunings), (override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };

    class FocSpeedMock
        : public FocSpeed
    {
    public:
        MOCK_METHOD(void, SetPolePairs, (std::size_t polePairs), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (RadiansPerSecond), (override));
        MOCK_METHOD(void, SetCurrentTunings, (Volts Vdc, const IdAndIqTunings& torqueTunings), (override));
        MOCK_METHOD(void, SetSpeedTunings, (Volts Vdc, const SpeedTunings& speedTuning), (override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };
}
