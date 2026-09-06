#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include <array>
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return arg.a.Value() == expected.a.Value() &&
               arg.b.Value() == expected.b.Value() &&
               arg.c.Value() == expected.c.Value();
    }

    class MotorAlignmentTest
        : public ::testing::Test
    {
    public:
        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        services::MotorAlignmentImpl alignment{ driverMock, encoderMock };
    };
}

TEST_F(MotorAlignmentTest, ForceAlignment_ConfiguresCorrectPwmDutyCycles)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    std::size_t polePairs = 7;

    foc::PhasePwmDutyCycles expectedPwm{
        hal::Percent{ 60 },
        hal::Percent{ 45 },
        hal::Percent{ 45 }
    };

    EXPECT_CALL(encoderMock, Read()).Times(1);
    EXPECT_CALL(driverMock, Stop()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(expectedPwm))).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 1000 }, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    bool callbackCalled = false;
    alignment.ForceAlignment(polePairs, config, [&callbackCalled](std::optional<foc::Radians>)
        {
            callbackCalled = true;
        });

    EXPECT_FALSE(callbackCalled);
}

TEST_F(MotorAlignmentTest, ForceAlignment_ReturnsNulloptWhenTimeoutOccurs)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 10;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    alignment.ForceAlignment(polePairs, config, [&result](std::optional<foc::Radians> offset)
        {
            result = offset;
        });

    for (std::size_t i = 0; i < config.maxSamples - 1; ++i)
    {
        EXPECT_CALL(encoderMock, Read())
            .WillOnce(Return(foc::Radians{ static_cast<float>(i) * 0.1f }));
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    }

    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(result.has_value());
}

TEST_F(MotorAlignmentTest, ForceAlignment_ConvergesWhenPositionStable)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 100;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    foc::Radians initialPosition{ 0.0f };
    foc::Radians stablePosition{ 1.5f };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(initialPosition));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(encoderMock, SetZero()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    alignment.ForceAlignment(polePairs, config, [&result](std::optional<foc::Radians> offset)
        {
            result = offset;
        });

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.5f }))
        .WillOnce(Return(foc::Radians{ 1.0f }))
        .WillOnce(Return(foc::Radians{ 1.4f }))
        .WillRepeatedly(Return(stablePosition));

    for (std::size_t i = 0; i < 4 + config.settledCount; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->Value(), stablePosition.Value(), 0.01f);
}

TEST_F(MotorAlignmentTest, ForceAlignment_CalculatesCorrectOffsetForDifferentPolePairs)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 3;
    std::size_t polePairs = 4;

    foc::Radians mechanicalPosition{ 0.785f };

    EXPECT_CALL(encoderMock, Read())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(mechanicalPosition));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(encoderMock, SetZero()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    alignment.ForceAlignment(polePairs, config, [&result](std::optional<foc::Radians> offset)
        {
            result = offset;
        });

    for (std::size_t i = 0; i < config.settledCount; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->Value(), mechanicalPosition.Value(), 0.001f);
}

TEST_F(MotorAlignmentTest, ForceAlignment_ResetsCounterWhenPositionChanges)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 100;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(encoderMock, SetZero()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    alignment.ForceAlignment(polePairs, config, [&result](std::optional<foc::Radians> offset)
        {
            result = offset;
        });

    foc::Radians finalPosition{ 1.5f };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 1.0f }))
        .WillOnce(Return(foc::Radians{ 1.0f }))
        .WillOnce(Return(foc::Radians{ 1.0f }))
        .WillOnce(Return(foc::Radians{ 1.5f }))
        .WillRepeatedly(Return(finalPosition));

    for (std::size_t i = 0; i < 4 + config.settledCount; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
}

TEST_F(MotorAlignmentTest, ForceAlignment_WithCustomVoltagePercent)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 30 };
    std::size_t polePairs = 7;

    foc::PhasePwmDutyCycles expectedPwm{
        hal::Percent{ 65 },
        hal::Percent{ 42 },
        hal::Percent{ 42 }
    };

    EXPECT_CALL(encoderMock, Read()).Times(1);
    EXPECT_CALL(driverMock, Stop()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(expectedPwm))).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 1000 }, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    bool callbackCalled = false;
    alignment.ForceAlignment(polePairs, config, [&callbackCalled](std::optional<foc::Radians>)
        {
            callbackCalled = true;
        });

    EXPECT_FALSE(callbackCalled);
}

TEST_F(MotorAlignmentTest, ForceAlignment_WithCustomSamplingFrequency)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.samplingFrequency = hal::Hertz{ 2000 };
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read()).Times(1);
    EXPECT_CALL(driverMock, Stop()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 2000 }, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    bool callbackCalled = false;
    alignment.ForceAlignment(polePairs, config, [&callbackCalled](std::optional<foc::Radians>)
        {
            callbackCalled = true;
        });

    EXPECT_FALSE(callbackCalled);
}

TEST_F(MotorAlignmentTest, ForceAlignment_ZeroesTheEncoderWhileHeldThenStopsBeforeCallback)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.settledCount = 2;
    std::size_t polePairs = 7;

    InSequence seq;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    bool callbackCalled = false;
    alignment.ForceAlignment(polePairs, config, [&callbackCalled](std::optional<foc::Radians>)
        {
            callbackCalled = true;
        });

    foc::Radians stablePosition{ 1.0f };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(stablePosition));
    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(stablePosition));
    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(stablePosition));
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(Return(stablePosition));
    EXPECT_CALL(encoderMock, SetZero()).Times(1);
    EXPECT_CALL(driverMock, Stop()).Times(1);
    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_TRUE(callbackCalled);
}

TEST_F(MotorAlignmentTest, ForceAlignment_WithZeroPosition)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.settledCount = 2;
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(encoderMock, SetZero()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    alignment.ForceAlignment(polePairs, config, [&result](std::optional<foc::Radians> offset)
        {
            result = offset;
        });

    for (std::size_t i = 0; i < config.settledCount; ++i)
        driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->Value(), 0.0f, 0.001f);
}

TEST_F(MotorAlignmentTest, ForceAlignment_AbortsWhenTheInjectedCurrentExceedsTheInverterLimit)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 100;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(2);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    std::optional<foc::Radians> result;
    bool called = false;
    alignment.ForceAlignment(polePairs, config, [&result, &called](std::optional<foc::Radians> offset)
        {
            called = true;
            result = offset;
        });

    const auto overLimit = drivers::ThreePhaseInverterMock::defaultMaxCurrent + 1.0f;
    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ overLimit }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_TRUE(called);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MotorAlignmentTest, ForceAlignment_AbortsOnOvercurrentInAnyPhaseAndInEitherDirection)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 100;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    const auto overLimit = drivers::ThreePhaseInverterMock::defaultMaxCurrent + 1.0f;

    const std::array<foc::PhaseCurrents, 2> offending{ {
        { foc::Ampere{ 0.0f }, foc::Ampere{ overLimit }, foc::Ampere{ 0.0f } },
        { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ -overLimit } },
    } };

    for (const auto& currents : offending)
    {
        StrictMock<drivers::ThreePhaseInverterMock> driver;
        StrictMock<drivers::EncoderMock> encoder;
        services::MotorAlignmentImpl subject{ driver, encoder };

        EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(driver, Stop()).Times(2);
        EXPECT_CALL(driver, ThreePhasePwmOutput(_));
        EXPECT_CALL(driver, PhaseCurrentsReady(_, _))
            .WillOnce([&driver](auto, const auto& onDone)
                {
                    driver.StorePhaseCurrentsCallback(onDone);
                });

        std::optional<foc::Radians> result;
        bool called = false;
        subject.ForceAlignment(polePairs, config, [&result, &called](std::optional<foc::Radians> offset)
            {
                called = true;
                result = offset;
            });

        driver.TriggerPhaseCurrentsCallback(currents);

        EXPECT_TRUE(called);
        EXPECT_FALSE(result.has_value());
    }
}

TEST_F(MotorAlignmentTest, ForceAlignment_ContinuesWhileTheInjectedCurrentStaysWithinTheLimit)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    config.testVoltagePercent = hal::Percent{ 20 };
    config.maxSamples = 100;
    config.settledThreshold = foc::Radians{ 0.001f };
    config.settledCount = 5;
    std::size_t polePairs = 7;

    EXPECT_CALL(encoderMock, Read()).Times(AtLeast(1)).WillRepeatedly(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop()).Times(1);
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& onDone)
            {
                driverMock.StorePhaseCurrentsCallback(onDone);
            });

    bool called = false;
    alignment.ForceAlignment(polePairs, config, [&called](std::optional<foc::Radians>)
        {
            called = true;
        });

    const auto withinLimit = drivers::ThreePhaseInverterMock::defaultMaxCurrent - 1.0f;
    driverMock.TriggerPhaseCurrentsCallback({ foc::Ampere{ withinLimit }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(called);
}

TEST_F(MotorAlignmentTest, AbortStopsTheDriverAndDropsTheCompletion)
{
    services::MotorAlignmentImpl::AlignmentConfig config;
    bool fired = false;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, Stop());
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });

    alignment.ForceAlignment(7, config, [&fired](auto)
        {
            fired = true;
        });

    EXPECT_CALL(driverMock, Stop());
    alignment.Abort();

    EXPECT_FALSE(fired);

    driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    EXPECT_FALSE(fired);
}

TEST_F(MotorAlignmentTest, AbortWithoutARunInFlightIsANoOp)
{
    alignment.Abort();
}
