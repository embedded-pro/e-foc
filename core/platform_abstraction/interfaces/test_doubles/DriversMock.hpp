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

    class EncoderIndexMock
        : public EncoderIndex
    {
    public:
        MOCK_METHOD(bool, HasIndex, (), (const, override));
        MOCK_METHOD(void, ArmIndex, (), (override));
        MOCK_METHOD(bool, IndexSeen, (), (override));
    };

    class ThreePhasePowerStageMock
        : public ThreePhasePowerStage
    {
    public:
        MOCK_METHOD(void, PhaseCurrentsReady, (const infra::Function<void(foc::PhaseCurrents phaseCurrents)>& onDone), (override));
        MOCK_METHOD(void, ThreePhasePwmOutput, ((const foc::PhasePwmDutyCycles&)), (override));
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (const, override));
        MOCK_METHOD(hal::Hertz, BaseFrequency, (), (const, override));
        MOCK_METHOD(void, CurrentOffsets, (const foc::PhaseCurrents& offsets), (override));
        MOCK_METHOD(void, ZeroVectorSlopes, (const foc::PhaseCurrentSlopes& slopes), (override));
    };

    class ThreePhaseInverterMock
        : public ThreePhaseInverter
    {
    public:
        ThreePhaseInverterMock()
        {
            ON_CALL(*this, MaxCurrentSupported()).WillByDefault(testing::Return(foc::Ampere{ defaultMaxCurrent }));
            EXPECT_CALL(*this, MaxCurrentSupported()).Times(testing::AnyNumber());
        }

        static constexpr float defaultMaxCurrent{ 10.0f };

        MOCK_METHOD(void, PhaseCurrentsReady, (hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents phaseCurrents)>& onDone), (override));
        MOCK_METHOD(void, ThreePhasePwmOutput, ((const foc::PhasePwmDutyCycles&)), (override));
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(foc::Ampere, MaxCurrentSupported, (), (const, override));
        MOCK_METHOD(hal::Hertz, BaseFrequency, (), (const, override));
        MOCK_METHOD(void, CurrentOffsets, (const foc::PhaseCurrents& offsets), (override));

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
