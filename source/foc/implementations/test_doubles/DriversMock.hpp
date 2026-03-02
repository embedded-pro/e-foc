#pragma once

#include "source/foc/interfaces/Driver.hpp"
#include <gmock/gmock.h>

namespace foc
{
    class EncoderMock
        : public Encoder
    {
    public:
        MOCK_METHOD(Radians, Read, (), (override));
        MOCK_METHOD(void, Set, (Radians value), (override));
        MOCK_METHOD(void, SetZero, (), (override));
    };

    class HallSensorMock
        : public HallSensor
    {
    public:
        MOCK_METHOD((std::pair<HallState, Direction>), Read, (), (const, override));
    };

    class FieldOrientedControllerInterfaceMock
        : public ThreePhaseInverter
    {
    public:
        MOCK_METHOD(void, PhaseCurrentsReady, (hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents phaseCurrents)>& onDone), (override));
        MOCK_METHOD(void, ThreePhasePwmOutput, ((const foc::PhasePwmDutyCycles&)), (override));
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(hal::Hertz, BaseFrequency, (), (const, override));

        void StorePhaseCurrentsCallback(const infra::Function<void(foc::PhaseCurrents phaseCurrents)>& onDone)
        {
            phaseCurrentsCallback = onDone;
        }

        void TriggerPhaseCurrentsCallback(foc::PhaseCurrents phaseCurrents)
        {
            if (phaseCurrentsCallback)
                phaseCurrentsCallback(phaseCurrents);
        }

    private:
        infra::Function<void(foc::PhaseCurrents phaseCurrents)> phaseCurrentsCallback;
    };
}
