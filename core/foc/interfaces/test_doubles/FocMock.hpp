#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include <gmock/gmock.h>

namespace foc
{
    class FocTorqueMock
        : public FocTorque
    {
    public:
        MOCK_METHOD(void, Configure, (const MotorModelParameters& parameters), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (IdAndIqPoint), (override));
        MOCK_METHOD(void, SetCurrentTunings, (const CurrentLoopTunings& tunings), (override));
        MOCK_METHOD(SelectResult, SelectCurrentAlgorithm, (CurrentAlgorithm algorithm), (override));
        MOCK_METHOD(CurrentAlgorithm, ActiveCurrentAlgorithm, (), (const, override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };

    class SpeedLoopTunableMock
        : public SpeedLoopTunable
    {
    public:
        MOCK_METHOD(void, ConfigureMechanics, (const MechanicalModelParameters& parameters), (override));
        MOCK_METHOD(void, SetSpeedTunings, (const SpeedLoopTunings& tunings), (override));
    };

    class FocSpeedMock
        : public FocSpeed
    {
    public:
        MOCK_METHOD(void, Configure, (const MotorModelParameters& parameters), (override));
        MOCK_METHOD(void, ConfigureMechanics, (const MechanicalModelParameters& parameters), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (RadiansPerSecond), (override));
        MOCK_METHOD(void, SetCurrentTunings, (const CurrentLoopTunings& tunings), (override));
        MOCK_METHOD(void, SetSpeedTunings, (const SpeedLoopTunings& tunings), (override));
        MOCK_METHOD(SelectResult, SelectCurrentAlgorithm, (CurrentAlgorithm algorithm), (override));
        MOCK_METHOD(CurrentAlgorithm, ActiveCurrentAlgorithm, (), (const, override));
        MOCK_METHOD(SelectResult, SelectSpeedAlgorithm, (SpeedAlgorithm algorithm), (override));
        MOCK_METHOD(SpeedAlgorithm, ActiveSpeedAlgorithm, (), (const, override));
        MOCK_METHOD(void, SetOnlineMechanicalEstimator, (OnlineMechanicalEstimator & estimator), (override));
        MOCK_METHOD(void, SetOnlineElectricalEstimator, (OnlineElectricalEstimator & estimator), (override));
        MOCK_METHOD(hal::Hertz, OuterLoopFrequency, (), (const, override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };

    class FocPositionMock
        : public FocPosition
    {
    public:
        MOCK_METHOD(void, Configure, (const MotorModelParameters& parameters), (override));
        MOCK_METHOD(void, ConfigureMechanics, (const MechanicalModelParameters& parameters), (override));
        MOCK_METHOD(void, Enable, (), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(void, SetPoint, (Radians), (override));
        MOCK_METHOD(void, SetCurrentTunings, (const CurrentLoopTunings& tunings), (override));
        MOCK_METHOD(void, SetSpeedTunings, (const SpeedLoopTunings& tunings), (override));
        MOCK_METHOD(void, SetPositionTunings, (const PositionLoopTunings& tunings), (override));
        MOCK_METHOD(SelectResult, SelectCurrentAlgorithm, (CurrentAlgorithm algorithm), (override));
        MOCK_METHOD(CurrentAlgorithm, ActiveCurrentAlgorithm, (), (const, override));
        MOCK_METHOD(SelectResult, SelectSpeedAlgorithm, (SpeedAlgorithm algorithm), (override));
        MOCK_METHOD(SpeedAlgorithm, ActiveSpeedAlgorithm, (), (const, override));
        MOCK_METHOD(void, SetOnlineMechanicalEstimator, (OnlineMechanicalEstimator & estimator), (override));
        MOCK_METHOD(void, SetOnlineElectricalEstimator, (OnlineElectricalEstimator & estimator), (override));
        MOCK_METHOD(PhasePwmDutyCycles, Calculate, (const PhaseCurrents& currentPhases, Radians& position), (override));
    };
}
