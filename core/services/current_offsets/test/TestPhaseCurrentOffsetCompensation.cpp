#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsetCompensation.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;

    const hal::Hertz baseFrequency{ 20000 };

    foc::PhaseCurrents Currents(float a, float b, float c)
    {
        return { foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } };
    }

    MATCHER_P(CurrentsNear, expected, "")
    {
        return std::abs(arg.a.Value() - expected.a.Value()) < 1e-6f &&
               std::abs(arg.b.Value() - expected.b.Value()) < 1e-6f &&
               std::abs(arg.c.Value() - expected.c.Value()) < 1e-6f;
    }

    MATCHER_P(DutiesEq, expected, "")
    {
        return arg.a == expected.a && arg.b == expected.b && arg.c == expected.c;
    }

    class TestPhaseCurrentOffsetCompensation
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestPhaseCurrentOffsetCompensation()
        {
            EXPECT_CALL(inverterMock, BaseFrequency()).WillRepeatedly(Return(baseFrequency));
        }

        void StartMeasurement()
        {
            EXPECT_CALL(inverterMock, PhaseCurrentsReady(baseFrequency, _)).WillOnce([this](auto, const auto& onDone)
                {
                    inverterMock.StorePhaseCurrentsCallback(onDone);
                });
            const auto neutral = hal::DutyCycle::FromPercent(50);
            EXPECT_CALL(inverterMock, ThreePhasePwmOutput(DutiesEq(foc::PhasePwmDutyCycles{ neutral, neutral, neutral })));

            compensation->Measure([this](std::optional<foc::PhaseCurrents> offsets)
                {
                    measured = offsets;
                    completed = true;
                });
        }

        void Feed(foc::PhaseCurrents currents, std::size_t count)
        {
            for (std::size_t sample = 0; sample != count; ++sample)
                inverterMock.TriggerPhaseCurrentsCallback(currents);
        }

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        infra::WithSharedAccess<services::PhaseCurrentOffsetCompensation> compensation{ inverterMock,
            services::PhaseCurrentOffsetCompensation::Config{ .settleSamples = 2, .averagedSamples = 4, .maxOffset = foc::Ampere{ 0.5f } } };
        std::optional<foc::PhaseCurrents> measured;
        bool completed{ false };
    };
}

TEST_F(TestPhaseCurrentOffsetCompensation, the_currents_it_forwards_have_the_applied_offsets_removed)
{
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(baseFrequency, _)).WillOnce([this](auto, const auto& onDone)
        {
            inverterMock.StorePhaseCurrentsCallback(onDone);
        });
    std::optional<foc::PhaseCurrents> forwarded;
    compensation->PhaseCurrentsReady(baseFrequency, [&forwarded](foc::PhaseCurrents currents)
        {
            forwarded = currents;
        });

    compensation->Apply(Currents(0.027f, -0.018f, 0.054f));
    inverterMock.TriggerPhaseCurrentsCallback(Currents(1.027f, -0.518f, -0.446f));

    ASSERT_TRUE(forwarded.has_value());
    EXPECT_THAT(*forwarded, CurrentsNear(Currents(1.0f, -0.5f, -0.5f)));
}

TEST_F(TestPhaseCurrentOffsetCompensation, the_bridge_commands_pass_through_unchanged)
{
    const foc::PhasePwmDutyCycles duties{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(40), hal::DutyCycle::FromPercent(50) };

    EXPECT_CALL(inverterMock, Start());
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(DutiesEq(duties)));
    EXPECT_CALL(inverterMock, Stop());

    compensation->Start();
    compensation->ThreePhasePwmOutput(duties);
    compensation->Stop();

    EXPECT_EQ(compensation->BaseFrequency(), baseFrequency);
    EXPECT_EQ(compensation->MaxCurrentSupported(), foc::Ampere{ drivers::ThreePhaseInverterMock::defaultMaxCurrent });
}

TEST_F(TestPhaseCurrentOffsetCompensation, a_measurement_averages_the_raw_currents_at_neutral_duty_after_they_settle)
{
    compensation->Apply(Currents(0.1f, 0.1f, 0.1f));
    StartMeasurement();

    Feed(Currents(0.4f, 0.4f, 0.4f), 2);
    Feed(Currents(0.02f, -0.01f, 0.06f), 2);
    EXPECT_CALL(inverterMock, Stop());
    Feed(Currents(0.04f, -0.03f, 0.04f), 2);
    ExecuteAllActions();

    ASSERT_TRUE(completed);
    ASSERT_TRUE(measured.has_value());
    EXPECT_THAT(*measured, CurrentsNear(Currents(0.03f, -0.02f, 0.05f)));
    EXPECT_FALSE(compensation->IsRunning());
}

TEST_F(TestPhaseCurrentOffsetCompensation, an_offset_beyond_the_limit_fails_the_measurement)
{
    StartMeasurement();

    Feed(Currents(0.0f, 0.0f, 0.0f), 2);
    EXPECT_CALL(inverterMock, Stop());
    Feed(Currents(0.0f, 0.6f, 0.0f), 4);
    ExecuteAllActions();

    EXPECT_TRUE(completed);
    EXPECT_FALSE(measured.has_value());
}

TEST_F(TestPhaseCurrentOffsetCompensation, a_current_beyond_the_inverter_limit_stops_the_bridge_and_fails)
{
    StartMeasurement();

    EXPECT_CALL(inverterMock, Stop());
    Feed(Currents(drivers::ThreePhaseInverterMock::defaultMaxCurrent + 1.0f, 0.0f, 0.0f), 1);
    Feed(Currents(0.0f, 0.0f, 0.0f), 6);
    ExecuteAllActions();

    EXPECT_TRUE(completed);
    EXPECT_FALSE(measured.has_value());
}

TEST_F(TestPhaseCurrentOffsetCompensation, a_bridge_that_sends_no_samples_fails_the_measurement)
{
    StartMeasurement();

    EXPECT_CALL(inverterMock, Stop());
    ForwardTime(std::chrono::milliseconds(100));

    EXPECT_TRUE(completed);
    EXPECT_FALSE(measured.has_value());
}

TEST_F(TestPhaseCurrentOffsetCompensation, an_aborted_measurement_stops_the_bridge_and_reports_nothing)
{
    StartMeasurement();
    Feed(Currents(0.0f, 0.0f, 0.0f), 3);

    EXPECT_CALL(inverterMock, Stop());
    compensation->Abort();
    Feed(Currents(0.0f, 0.0f, 0.0f), 3);
    ExecuteAllActions();

    EXPECT_FALSE(completed);
    EXPECT_FALSE(compensation->IsRunning());
}
