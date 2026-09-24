#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/electrical_system_ident/ResistanceEstimator.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;

    bool WithinOneStep(hal::DutyCycle actual, hal::DutyCycle expected)
    {
        return (actual.Value() > expected.Value() ? actual.Value() - expected.Value() : expected.Value() - actual.Value()) <= 1;
    }

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return WithinOneStep(arg.a, expected.a) &&
               WithinOneStep(arg.b, expected.b) &&
               WithinOneStep(arg.c, expected.c);
    }

    class ResistanceEstimatorTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        foc::Volts vdc{ 24.0f };
        services::ResistanceEstimator estimator{ driverMock, vdc };
    };
}

TEST_F(ResistanceEstimatorTest, start_applies_test_voltage_and_registers_blank_callback)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::seconds{ 1 }, services::WindingConfiguration::Wye
    };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, _));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(foc::PhasePwmDutyCycles{
                                hal::DutyCycle::FromPercent(15), hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1) })));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [](auto) {});
}

TEST_F(ResistanceEstimatorTest, registers_sampling_callback_after_settle_time)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(20), std::chrono::milliseconds{ 100 }, services::WindingConfiguration::Wye,
        std::chrono::seconds{ 1 }
    };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(foc::PhasePwmDutyCycles{
                                hal::DutyCycle::FromPercent(20), hal::DutyCycle::FromPercent(1), hal::DutyCycle::FromPercent(1) })));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [](auto) {});
    ForwardTime(std::chrono::milliseconds{ 100 });
}

TEST_F(ResistanceEstimatorTest, recovers_resistance_from_settled_current)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye
    };

    const float testVoltage = 0.14f * vdc.Value();
    const float terminalR = 1.5f;
    const float steadyStateCurrent = testVoltage / terminalR;

    services::ResistanceEstimator::Result result{};

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < 127; ++i)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ steadyStateCurrent }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.resistance.has_value());
    EXPECT_NEAR(result.resistance->Value(), terminalR / 1.5f, 0.01f);
}

TEST_F(ResistanceEstimatorTest, returns_no_resistance_for_zero_current)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(10), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye
    };

    services::ResistanceEstimator::Result result{ foc::Ohm{ 1.0f } };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < 127; ++i)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(result.resistance.has_value());
}

TEST_F(ResistanceEstimatorTest, an_overcurrent_sample_while_settling_aborts_with_an_empty_result)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye
    };

    std::optional<services::ResistanceEstimator::Result> result;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });

    driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
        foc::Ampere{ drivers::ThreePhaseInverterMock::defaultMaxCurrent - 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    EXPECT_FALSE(result.has_value());

    driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
        foc::Ampere{ drivers::ThreePhaseInverterMock::defaultMaxCurrent + 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->resistance.has_value());

    ForwardTime(std::chrono::milliseconds{ 50 });
}

TEST_F(ResistanceEstimatorTest, an_overcurrent_sample_while_measuring_aborts_with_an_empty_result)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye
    };

    std::optional<services::ResistanceEstimator::Result> result;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
        foc::Ampere{ drivers::ThreePhaseInverterMock::defaultMaxCurrent + 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->resistance.has_value());

    driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
        foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
}

TEST_F(ResistanceEstimatorTest, recovers_resistance_for_low_resistance_motor)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye
    };

    const float testVoltage = 0.14f * vdc.Value();
    const float terminalR = 0.5f;
    const float steadyStateCurrent = testVoltage / terminalR;

    services::ResistanceEstimator::Result result{};

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_)).Times(1);

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < 127; ++i)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ steadyStateCurrent }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ASSERT_TRUE(result.resistance.has_value());
    EXPECT_NEAR(result.resistance->Value(), terminalR / 1.5f, 0.01f);
}

TEST_F(ResistanceEstimatorTest, no_sample_timeout_aborts_if_no_adc_callback_after_settle)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye,
        std::chrono::milliseconds{ 100 }
    };

    std::optional<services::ResistanceEstimator::Result> result;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(config.settleTime);

    ForwardTime(2 * config.noSampleTimeout + std::chrono::milliseconds{ 1 });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->resistance.has_value());
}

TEST_F(ResistanceEstimatorTest, sample_watchdog_fires_once_samples_stop_arriving)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::milliseconds{ 50 }, services::WindingConfiguration::Wye,
        std::chrono::milliseconds{ 100 }
    };

    std::optional<services::ResistanceEstimator::Result> result;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });
    ForwardTime(config.settleTime);

    for (std::size_t i = 0; i < 5; ++i)
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    ForwardTime(2 * config.noSampleTimeout + std::chrono::milliseconds{ 1 });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->resistance.has_value());
}

TEST_F(ResistanceEstimatorTest, no_sample_timeout_fires_during_settle_if_no_adc_callback)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::seconds{ 2 }, services::WindingConfiguration::Wye,
        std::chrono::milliseconds{ 100 }
    };

    std::optional<services::ResistanceEstimator::Result> result;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _))
        .WillOnce([this](auto, const auto& cb)
            {
                driverMock.StorePhaseCurrentsCallback(cb);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    estimator.Start(config, [&result](auto r)
        {
            result = r;
        });

    ForwardTime(2 * config.noSampleTimeout + std::chrono::milliseconds{ 1 });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->resistance.has_value());
}

TEST_F(ResistanceEstimatorTest, destructor_stops_driver_when_destroyed_while_active)
{
    services::ResistanceEstimator::Config config{
        hal::DutyCycle::FromPercent(15), std::chrono::seconds{ 2 }, services::WindingConfiguration::Wye,
        std::chrono::seconds{ 5 }
    };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(driverMock, Stop());

    {
        services::ResistanceEstimator local{ driverMock, foc::Volts{ 24.0f } };
        local.Start(config, [](auto) {});
    }
}
