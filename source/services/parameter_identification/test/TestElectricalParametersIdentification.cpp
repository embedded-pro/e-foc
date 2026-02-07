#include "infra/timer/test_helper/ClockFixture.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/services/parameter_identification/ElectricalParametersIdentificationImpl.hpp"
#include <cmath>
#include <gmock/gmock.h>

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

        StrictMock<foc::FieldOrientedControllerInterfaceMock> driverMock;
        StrictMock<foc::EncoderMock> encoderMock;
        foc::Volts vdc{ 24.0f };
        services::ElectricalParametersIdentificationImpl identification{ driverMock, encoderMock, vdc };
    };
}

TEST_F(ElectricalParametersIdentificationTest, estimate_resistance_and_inductance_starts_with_neutral_duty_and_settles)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 15 },
        std::chrono::seconds{ 1 },
        services::WindingConfiguration::Wye
    };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(hal::Hertz{ 10000 }, ::testing::_));
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(foc::PhasePwmDutyCycles{
                                hal::Percent{ 1 },
                                hal::Percent{ 1 },
                                hal::Percent{ 1 } })));

    identification.EstimateResistanceAndInductance(config, [](auto, auto) {});
}

TEST_F(ElectricalParametersIdentificationTest, estimate_resistance_and_inductance_applies_test_voltage_after_settle_time)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 20 },
        std::chrono::milliseconds{ 100 },
        services::WindingConfiguration::Wye
    };

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& callback)
            {
                driverMock.StorePhaseCurrentsCallback(callback);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(foc::PhasePwmDutyCycles{
                                hal::Percent{ 1 },
                                hal::Percent{ 1 },
                                hal::Percent{ 1 } })));

    identification.EstimateResistanceAndInductance(config, [](auto, auto) {});

    EXPECT_CALL(driverMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(foc::PhasePwmDutyCycles{
                                hal::Percent{ 20 },
                                hal::Percent{ 1 },
                                hal::Percent{ 1 } })));

    ForwardTime(std::chrono::milliseconds{ 100 });
}

TEST_F(ElectricalParametersIdentificationTest, estimate_resistance_and_inductance_collects_current_samples_and_calculates_parameters)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 15 },
        std::chrono::milliseconds{ 50 },
        services::WindingConfiguration::Wye
    };

    float testVoltage = 0.15f * vdc.Value();
    float resistance = 1.5f;
    float inductance = 0.002f;

    std::optional<foc::Ohm> resultResistance;
    std::optional<foc::MilliHenry> resultInductance;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& callback)
            {
                driverMock.StorePhaseCurrentsCallback(callback);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(2);

    identification.EstimateResistanceAndInductance(config, [&](auto r, auto l)
        {
            resultResistance = r;
            resultInductance = l;
        });

    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < numberOfSamples; ++i)
    {
        float time = static_cast<float>(i) * 0.0001f;
        float current = SimulateRLModelCurrent(testVoltage, resistance, inductance, time);
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ current },
            foc::Ampere{ 0.0f },
            foc::Ampere{ 0.0f } });
    }

    ASSERT_TRUE(resultResistance.has_value());
    ASSERT_TRUE(resultInductance.has_value());
    EXPECT_NEAR(resultResistance->Value(), resistance, 0.1f);
    EXPECT_NEAR(resultInductance->Value(), inductance * 1000.0f, 1.0f);
}

TEST_F(ElectricalParametersIdentificationTest, estimate_resistance_and_inductance_returns_nullopt_for_zero_current)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 10 },
        std::chrono::milliseconds{ 50 },
        services::WindingConfiguration::Wye
    };

    std::optional<foc::Ohm> resultResistance;
    std::optional<foc::MilliHenry> resultInductance;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& callback)
            {
                driverMock.StorePhaseCurrentsCallback(callback);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(2);

    identification.EstimateResistanceAndInductance(config, [&](auto r, auto l)
        {
            resultResistance = r;
            resultInductance = l;
        });

    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < numberOfSamples; ++i)
    {
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 0.0f },
            foc::Ampere{ 0.0f },
            foc::Ampere{ 0.0f } });
    }

    EXPECT_FALSE(resultResistance.has_value());
    EXPECT_FALSE(resultInductance.has_value());
}

TEST_F(ElectricalParametersIdentificationTest, estimate_resistance_and_inductance_with_low_resistance_motor)
{
    services::ElectricalParametersIdentification::ResistanceAndInductanceConfig config{
        hal::Percent{ 15 },
        std::chrono::milliseconds{ 50 },
        services::WindingConfiguration::Wye
    };

    float testVoltage = 0.15f * 24.0f;
    float resistance = 0.5f;
    float inductance = 0.001f;

    std::optional<foc::Ohm> resultResistance;
    std::optional<foc::MilliHenry> resultInductance;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly([this](auto, const auto& callback)
            {
                driverMock.StorePhaseCurrentsCallback(callback);
            });
    EXPECT_CALL(driverMock, ThreePhasePwmOutput(::testing::_))
        .Times(2);

    identification.EstimateResistanceAndInductance(config, [&](auto r, auto l)
        {
            resultResistance = r;
            resultInductance = l;
        });

    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < numberOfSamples; ++i)
    {
        float time = static_cast<float>(i) * 0.0001f;
        float current = SimulateRLModelCurrent(testVoltage, resistance, inductance, time);
        driverMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ current },
            foc::Ampere{ 0.0f },
            foc::Ampere{ 0.0f } });
    }

    ASSERT_TRUE(resultResistance.has_value());
    ASSERT_TRUE(resultInductance.has_value());
    EXPECT_NEAR(resultResistance->Value(), resistance, 0.15f);
    EXPECT_NEAR(resultInductance->Value(), inductance * 1000.0f, 0.5f);
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
