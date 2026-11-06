#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/index_search/IndexSearchImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>
#include <optional>

namespace
{
    using namespace testing;

    constexpr uint32_t baseFrequency{ 1000 };
    constexpr std::size_t holdSamples{ 200 };
    constexpr std::size_t polePairs{ 4 };
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float stepPerSample = pi * static_cast<float>(polePairs) / static_cast<float>(baseFrequency);

    bool WithinOneStep(hal::DutyCycle actual, hal::DutyCycle expected)
    {
        return (actual.Value() > expected.Value() ? actual.Value() - expected.Value() : expected.Value() - actual.Value()) <= 1;
    }

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return WithinOneStep(arg.a, expected.a) && WithinOneStep(arg.b, expected.b) && WithinOneStep(arg.c, expected.c);
    }

    foc::PhasePwmDutyCycles VectorAt(float amplitude, float electricalAngle)
    {
        const auto a = amplitude * std::cos(electricalAngle);
        const auto b = amplitude * std::cos(electricalAngle - 2.0f * pi / 3.0f);
        const auto c = amplitude * std::cos(electricalAngle + 2.0f * pi / 3.0f);
        return foc::ToDutyCycles(foc::ThreePhase{ 0.5f + 0.5f * a, 0.5f + 0.5f * b, 0.5f + 0.5f * c });
    }

    class IndexSearchTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        IndexSearchTest()
        {
            EXPECT_CALL(driver, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ baseFrequency }));
        }

        void TearDown() override
        {
            ExecuteAllActions();
        }

        services::IndexSearch::Config Config(std::optional<foc::Radians> target = std::nullopt) const
        {
            return services::IndexSearch::Config{ .polePairs = polePairs, .phaseResistance = foc::Ohm{ 4.0f }, .busVoltage = foc::Volts{ 24.0f }, .current = foc::Ampere{ 0.6f }, .target = target };
        }

        void Start(std::optional<foc::Radians> target = std::nullopt)
        {
            EXPECT_CALL(driver, Stop());
            EXPECT_CALL(driver, ThreePhasePwmOutput(_));
            EXPECT_CALL(driver, PhaseCurrentsReady(hal::Hertz{ baseFrequency }, _)).WillOnce([this](auto, const auto& onDone)
                {
                    driver.StorePhaseCurrentsCallback(onDone);
                });

            search->Search(Config(target), [this](std::optional<foc::Radians> reading)
                {
                    result = reading;
                    ++completions;
                });
        }

        void Sample(float current = 0.0f)
        {
            driver.TriggerPhaseCurrentsCallback({ foc::Ampere{ current }, foc::Ampere{ -0.5f * current }, foc::Ampere{ -0.5f * current } });
        }

        void Hold()
        {
            for (std::size_t sample = 1; sample < holdSamples; ++sample)
                Sample();

            EXPECT_CALL(index, ArmIndex());
            Sample();
        }

        void TurnWithoutIndex(std::size_t samples)
        {
            EXPECT_CALL(index, IndexSeen()).Times(samples).WillRepeatedly(Return(false));
            EXPECT_CALL(driver, ThreePhasePwmOutput(_)).Times(samples);

            for (std::size_t sample = 0; sample != samples; ++sample)
                Sample();
        }

        StrictMock<drivers::ThreePhaseInverterMock> driver;
        StrictMock<drivers::EncoderMock> encoder;
        StrictMock<drivers::EncoderIndexMock> index;
        infra::WithSharedAccess<services::IndexSearchImpl> search{ driver, encoder, index };
        std::optional<foc::Radians> result;
        std::size_t completions{ 0 };
    };
}

TEST_F(IndexSearchTest, is_available_only_with_an_index_channel)
{
    EXPECT_CALL(index, HasIndex()).WillOnce(Return(true)).WillOnce(Return(false));

    EXPECT_TRUE(search->Available());
    EXPECT_FALSE(search->Available());
}

TEST_F(IndexSearchTest, refuses_without_resistance_or_bus_voltage)
{
    auto config = Config();
    config.phaseResistance = foc::Ohm{ 0.0f };

    search->Search(config, [this](std::optional<foc::Radians> reading)
        {
            result = reading;
            ++completions;
        });
    config = Config();
    config.busVoltage = foc::Volts{ 0.0f };
    search->Search(config, [this](std::optional<foc::Radians> reading)
        {
            result = reading;
            ++completions;
        });

    EXPECT_EQ(completions, 2u);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(search->IsRunning());
}

TEST_F(IndexSearchTest, holds_a_vector_sized_for_the_search_current_at_electrical_zero)
{
    EXPECT_CALL(driver, Stop());
    EXPECT_CALL(driver, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(VectorAt(0.2f, 0.0f))));
    EXPECT_CALL(driver, PhaseCurrentsReady(hal::Hertz{ baseFrequency }, _));

    search->Search(Config(), [](std::optional<foc::Radians>) {});

    EXPECT_TRUE(search->IsRunning());
    EXPECT_CALL(driver, Stop());
    search->Abort();
}

TEST_F(IndexSearchTest, limits_the_search_current_to_a_quarter_of_the_inverter)
{
    EXPECT_CALL(driver, MaxCurrentSupported()).WillRepeatedly(Return(foc::Ampere{ 1.2f }));
    EXPECT_CALL(driver, Stop());
    EXPECT_CALL(driver, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(VectorAt(0.1f, 0.0f))));
    EXPECT_CALL(driver, PhaseCurrentsReady(_, _));

    search->Search(Config(), [](std::optional<foc::Radians>) {});

    EXPECT_CALL(driver, Stop());
    search->Abort();
}

TEST_F(IndexSearchTest, arms_the_index_only_when_the_hold_ends)
{
    Start();

    for (std::size_t sample = 1; sample < holdSamples; ++sample)
        Sample();

    EXPECT_CALL(index, ArmIndex());
    Sample();

    EXPECT_CALL(driver, Stop());
    search->Abort();
}

TEST_F(IndexSearchTest, turns_the_vector_forward_at_half_a_revolution_per_second)
{
    Start();
    Hold();

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(false));
    EXPECT_CALL(driver, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(VectorAt(0.2f, stepPerSample))));
    Sample();

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(false));
    EXPECT_CALL(driver, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(VectorAt(0.2f, 2.0f * stepPerSample))));
    Sample();

    EXPECT_CALL(driver, Stop());
    search->Abort();
}

TEST_F(IndexSearchTest, reports_the_reading_at_the_index_after_stopping_the_inverter)
{
    Start();
    Hold();
    TurnWithoutIndex(10);

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(true));
    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ 1.25f }));
    Sample();
    Sample();
    EXPECT_EQ(completions, 0u);

    EXPECT_CALL(driver, Stop());
    ExecuteAllActions();

    ASSERT_EQ(completions, 1u);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->Value(), 1.25f, 1e-6f);
    EXPECT_FALSE(search->IsRunning());
}

TEST_F(IndexSearchTest, sets_the_encoder_to_read_the_target_at_the_index)
{
    Start(foc::Radians{ 2.5f });
    Hold();

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(true));
    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ -1.0f }));
    Sample();

    InSequence sequence;
    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ -0.98f }));
    EXPECT_CALL(encoder, Set(Property(&foc::Radians::Value, FloatNear(2.52f, 1e-5f))));
    EXPECT_CALL(driver, Stop());
    ExecuteAllActions();

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->Value(), -1.0f, 1e-6f);
}

TEST_F(IndexSearchTest, wraps_the_reference_into_one_revolution)
{
    Start(foc::Radians{ 3.0f });
    Hold();

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(true));
    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ -3.0f }));
    Sample();

    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ -2.8f }));
    EXPECT_CALL(encoder, Set(Property(&foc::Radians::Value, FloatNear(3.2f - 2.0f * pi, 1e-5f))));
    EXPECT_CALL(driver, Stop());
    ExecuteAllActions();
}

TEST_F(IndexSearchTest, fails_after_a_revolution_and_a_quarter_without_an_index)
{
    Start();
    Hold();

    const auto travelSamples = static_cast<std::size_t>(2.5f * baseFrequency);
    TurnWithoutIndex(travelSamples);
    EXPECT_EQ(completions, 0u);

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(false));
    EXPECT_CALL(driver, Stop());
    Sample();
    Sample();

    ExecuteAllActions();
    ASSERT_EQ(completions, 1u);
    EXPECT_FALSE(result.has_value());
}

TEST_F(IndexSearchTest, fails_on_a_phase_current_above_twice_the_search_current)
{
    Start();
    Sample(1.2f);

    EXPECT_CALL(driver, Stop());
    Sample(1.21f);
    Sample(0.0f);

    ExecuteAllActions();
    ASSERT_EQ(completions, 1u);
    EXPECT_FALSE(result.has_value());
}

TEST_F(IndexSearchTest, fails_after_four_seconds)
{
    Start();

    EXPECT_CALL(driver, Stop());
    ForwardTime(std::chrono::seconds{ 4 });

    ASSERT_EQ(completions, 1u);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(search->IsRunning());
}

TEST_F(IndexSearchTest, abort_drops_the_completion_and_leaves_the_encoder_untouched)
{
    Start(foc::Radians{ 1.0f });
    Hold();

    EXPECT_CALL(index, IndexSeen()).WillOnce(Return(true));
    EXPECT_CALL(encoder, Read()).WillOnce(Return(foc::Radians{ 0.5f }));
    Sample();

    EXPECT_CALL(driver, Stop());
    search->Abort();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds{ 5 });

    EXPECT_EQ(completions, 0u);
    EXPECT_FALSE(search->IsRunning());
}

TEST_F(IndexSearchTest, a_second_search_while_running_is_refused)
{
    Start();

    std::optional<std::optional<foc::Radians>> second;
    search->Search(Config(), [&second](std::optional<foc::Radians> reading)
        {
            second = reading;
        });

    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(second->has_value());
    EXPECT_TRUE(search->IsRunning());

    EXPECT_CALL(driver, Stop());
    search->Abort();
}
