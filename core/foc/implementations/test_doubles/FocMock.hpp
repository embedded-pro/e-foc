#pragma once

#include "core/foc/interfaces/Foc.hpp"
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

    class FocSpeedTunableMock
        : public FocSpeedTunable
    {
    public:
        MOCK_METHOD(void, SetSpeedTunings, (Volts Vdc, const SpeedTunings& speedTuning), (override));
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
        MOCK_METHOD(void, SetOnlineMechanicalEstimator, (OnlineMechanicalEstimator & estimator), (override));
        MOCK_METHOD(void, SetOnlineElectricalEstimator, (OnlineElectricalEstimator & estimator), (override));
        MOCK_METHOD(hal::Hertz, OuterLoopFrequency, (), (const, override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };

    class FocPositionMock
        : public FocPosition
    {
    public:
        MOCK_METHOD(void, SetPolePairs, (std::size_t polePairs), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (Radians), (override));
        MOCK_METHOD(void, SetCurrentTunings, (Volts Vdc, const IdAndIqTunings& torqueTunings), (override));
        MOCK_METHOD(void, SetSpeedTunings, (Volts Vdc, const SpeedTunings& speedTuning), (override));
        MOCK_METHOD(void, SetPositionTunings, (const PositionTunings& positionTuning), (override));
        MOCK_METHOD(void, SetOnlineMechanicalEstimator, (OnlineMechanicalEstimator & estimator), (override));
        MOCK_METHOD(void, SetOnlineElectricalEstimator, (OnlineElectricalEstimator & estimator), (override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };
}
