#pragma once

#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include <gmock/gmock.h>

namespace drivers
{
    class EncoderMock
        : public Encoder
    {
    public:
        MOCK_METHOD(foc::Radians, Read, (), (override));
        MOCK_METHOD(void, Set, (foc::Radians value), (override));
        MOCK_METHOD(void, SetZero, (), (override));
    };

    class HallSensorMock
        : public HallSensor
    {
    public:
        MOCK_METHOD((std::pair<foc::HallState, foc::Direction>), Read, (), (const, override));
    };

    class ThreePhaseInverterMock
        : public ThreePhaseInverter
    {
    public:
        MOCK_METHOD(void, PhaseCurrentsReady, (hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents phaseCurrents)>& onDone), (override));
        MOCK_METHOD(void, ThreePhasePwmOutput, ((const foc::PhasePwmDutyCycles&)), (override));
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (const, override));
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
