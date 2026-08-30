#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return arg.a.Value() == expected.a.Value() &&
               arg.b.Value() == expected.b.Value() &&
               arg.c.Value() == expected.c.Value();
    }

    float SimulateRLModelCurrent(float voltage, float resistance, float inductance, float time)
    {
        auto tau = inductance / resistance;

        return (voltage / resistance) * (1.0f - std::exp(-time / tau));
    }

    float MechanicalAngle(std::size_t stepIndex, std::size_t totalSteps, std::size_t expectedPolePairs)
    {
        constexpr std::size_t stepsPerRevolution = 12;
        auto electricalRevolutions = totalSteps / stepsPerRevolution;
        auto electricalAngle = (static_cast<float>(stepIndex) / static_cast<float>(totalSteps)) * (static_cast<float>(electricalRevolutions) * 2.0f * std::numbers::pi_v<float>);
        return electricalAngle / static_cast<float>(expectedPolePairs);
    }

    class ElectricalParametersIdentificationTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        const std::size_t numberOfSamples = 127;
        std::size_t encoderStepIndex = 0;

        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        foc::Volts vdc{ 24.0f };
        services::ElectricalParametersIdentificationImpl identification{ driverMock, encoderMock, vdc };
    };
}



TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_initializes_encoder_and_applies_voltages)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_));

    identification.EstimateNumberOfPolePairs(config, [](auto) {});
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_calculates_correct_pole_pairs_for_4_pole_motor)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;
    constexpr std::size_t expectedPolePairs = 2;
    float voltage = static_cast<float>(config.testVoltagePercent.Value()) * vdc.Value() / 100.0f;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_calculates_correct_pole_pairs_for_6_pole_motor)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;
    constexpr std::size_t expectedPolePairs = 3;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_returns_nullopt_for_insufficient_rotation)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(::testing::Return(foc::Radians{ 0.0f }));

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_FALSE(resultPolePairs.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_with_different_electrical_revolutions)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        10,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 10 * 12;
    constexpr std::size_t expectedPolePairs = 4;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}

TEST_F(ElectricalParametersIdentificationTest, concurrent_rl_estimate_is_rejected_immediately)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 15 }, std::chrono::milliseconds{ 100 }, services::WindingConfiguration::Wye
    };

    struct RlResult
    {
        bool called = false;
        services::ElectricalParametersIdentification::ResistanceInductanceResult result{};
    } second;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_));

    identification.EstimateResistanceAndInductance(config, [](services::ElectricalParametersIdentification::ResistanceInductanceResult) {});

    // Second call while first is in-flight must return immediately with nullopt
    identification.EstimateResistanceAndInductance(config, [&second](services::ElectricalParametersIdentification::ResistanceInductanceResult r)
        {
            second.called = true;
            second.result = r;
        });

    EXPECT_TRUE(second.called);
    EXPECT_FALSE(second.result.resistance.has_value());
    EXPECT_FALSE(second.result.inductance.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, concurrent_pole_pairs_estimate_is_rejected_immediately)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 }, 5, std::chrono::milliseconds{ 50 }
    };

    struct PpResult { bool called = false; bool hasValue = true; } second;

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_));

    identification.EstimateNumberOfPolePairs(config, [](auto) {});

    // Second call while first is in-flight must return immediately with nullopt
    identification.EstimateNumberOfPolePairs(config, [&second](auto result)
        {
            second.called = true;
            second.hasValue = result.has_value();
        });

    EXPECT_TRUE(second.called);
    EXPECT_FALSE(second.hasValue);
}

TEST_F(ElectricalParametersIdentificationTest, estimate_number_of_pole_pairs_with_8_pole_motor)
{
    services::ElectricalParametersIdentification::PolePairsConfig config{
        hal::Percent{ 20 },
        5,
        std::chrono::milliseconds{ 50 }
    };

    std::optional<std::size_t> resultPolePairs;
    constexpr std::size_t totalSteps = 5 * 12;
    constexpr std::size_t expectedPolePairs = 4;

    encoderStepIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }))
        .WillRepeatedly([this, totalSteps, expectedPolePairs]()
            {
                ++encoderStepIndex;
                return foc::Radians{ MechanicalAngle(encoderStepIndex, totalSteps, expectedPolePairs) };
            });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(totalSteps);
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateNumberOfPolePairs(config, [&](auto result)
        {
            resultPolePairs = result;
        });

    for (std::size_t i = 0; i < totalSteps; ++i)
        ForwardTime(std::chrono::milliseconds{ 50 });

    ASSERT_TRUE(resultPolePairs.has_value());
    EXPECT_EQ(*resultPolePairs, expectedPolePairs);
}
