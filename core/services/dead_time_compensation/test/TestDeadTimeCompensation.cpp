#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/model/PowerStage.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/dead_time_compensation/DeadTimeCompensation.hpp"
#include <chrono>
#include <cmath>
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;
    using namespace std::chrono_literals;

    const hal::Hertz baseFrequency{ 20000 };
    const drivers::DeadTimeLoss benchLoss{ 70ns, foc::Ampere{ 0.2f } };
    // 70 ns of each 50 us period, in the duty's fixed-point counts
    constexpr float lossCounts{ 70e-9f * 20000.0f * static_cast<float>(hal::DutyCycle::fullScale) };

    foc::PhaseCurrents Currents(float a, float b, float c)
    {
        return { foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } };
    }

    hal::DutyCycle Shifted(hal::DutyCycle duty, float counts)
    {
        return hal::DutyCycle{ static_cast<uint32_t>(static_cast<float>(duty.Value()) + counts + 0.5f) };
    }

    MATCHER_P(DutiesEq, expected, "")
    {
        return arg.a == expected.a && arg.b == expected.b && arg.c == expected.c;
    }

    class TestDeadTimeCompensation
        : public ::testing::Test
    {
    public:
        explicit TestDeadTimeCompensation(const drivers::DeadTimeLoss& loss = benchLoss)
            : compensation{ inverterMock, loss }
        {}

        void Register(hal::Hertz frequency = baseFrequency)
        {
            EXPECT_CALL(inverterMock, PhaseCurrentsReady(frequency, _)).WillOnce([this](auto, const auto& onDone)
                {
                    inverterMock.StorePhaseCurrentsCallback(onDone);
                });
            compensation.PhaseCurrentsReady(frequency, [this](foc::PhaseCurrents currents)
                {
                    forwarded = currents;
                });
        }

        foc::PhasePwmDutyCycles Output(const foc::PhasePwmDutyCycles& duties)
        {
            foc::PhasePwmDutyCycles written{};
            EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_)).WillOnce(SaveArg<0>(&written));
            compensation.ThreePhasePwmOutput(duties);
            return written;
        }

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        services::DeadTimeCompensation compensation;
        std::optional<foc::PhaseCurrents> forwarded;
        const hal::DutyCycle half{ hal::DutyCycle::FromPercent(50) };
    };

    class TestDeadTimeCompensationWithoutLoss
        : public TestDeadTimeCompensation
    {
    public:
        TestDeadTimeCompensationWithoutLoss()
            : TestDeadTimeCompensation(drivers::DeadTimeLoss{})
        {}
    };
}

TEST_F(TestDeadTimeCompensation, the_currents_pass_through_unchanged)
{
    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(1.0f, -0.4f, -0.6f));

    ASSERT_TRUE(forwarded.has_value());
    EXPECT_EQ(forwarded->a, foc::Ampere{ 1.0f });
    EXPECT_EQ(forwarded->b, foc::Ampere{ -0.4f });
    EXPECT_EQ(forwarded->c, foc::Ampere{ -0.6f });
}

TEST_F(TestDeadTimeCompensation, the_bridge_commands_pass_through_unchanged)
{
    EXPECT_CALL(inverterMock, Start());
    EXPECT_CALL(inverterMock, Stop());
    EXPECT_CALL(inverterMock, BaseFrequency()).WillOnce(Return(baseFrequency));

    compensation.Start();
    compensation.Stop();

    EXPECT_EQ(compensation.BaseFrequency(), baseFrequency);
    EXPECT_EQ(compensation.MaxCurrentSupported(), foc::Ampere{ drivers::ThreePhaseInverterMock::defaultMaxCurrent });
}

TEST_F(TestDeadTimeCompensation, a_leg_gains_the_whole_loss_while_a_current_past_the_knee_flows_out_of_it_and_gives_it_up_while_one_flows_in)
{
    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(1.0f, -0.5f, 0.0f));

    EXPECT_THAT(Output({ half, half, half }), DutiesEq(foc::PhasePwmDutyCycles{ Shifted(half, lossCounts), Shifted(half, -lossCounts), half }));
}

TEST_F(TestDeadTimeCompensation, below_the_knee_the_correction_follows_the_current)
{
    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(0.1f, -0.05f, -0.05f));

    EXPECT_THAT(Output({ half, half, half }), DutiesEq(foc::PhasePwmDutyCycles{ Shifted(half, 0.5f * lossCounts), Shifted(half, -0.25f * lossCounts), Shifted(half, -0.25f * lossCounts) }));
}

TEST_F(TestDeadTimeCompensation, the_correction_is_a_fixed_time_so_a_slower_pwm_needs_less_duty)
{
    Register(hal::Hertz{ 10000 });
    inverterMock.TriggerPhaseCurrentsCallback(Currents(1.0f, -1.0f, 0.0f));

    EXPECT_THAT(Output({ half, half, half }), DutiesEq(foc::PhasePwmDutyCycles{ Shifted(half, 0.5f * lossCounts), Shifted(half, -0.5f * lossCounts), half }));
}

TEST_F(TestDeadTimeCompensation, the_corrected_duty_stays_within_the_period)
{
    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(-1.0f, 1.0f, 0.0f));

    const hal::DutyCycle full{ hal::DutyCycle::fullScale };
    EXPECT_THAT(Output({ hal::DutyCycle{ 0 }, full, half }), DutiesEq(foc::PhasePwmDutyCycles{ hal::DutyCycle{ 0 }, full, half }));
}

TEST_F(TestDeadTimeCompensationWithoutLoss, an_unknown_loss_leaves_the_duties_alone)
{
    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(2.0f, -1.0f, -1.0f));

    const foc::PhasePwmDutyCycles duties{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(45), hal::DutyCycle::FromPercent(45) };
    EXPECT_THAT(Output(duties), DutiesEq(duties));
}

TEST_F(TestDeadTimeCompensation, through_a_stage_that_loses_the_dead_time_the_corrected_leg_delivers_the_commanded_voltage)
{
    constexpr float busVoltage{ 24.0f };
    constexpr float period{ 50e-6f };
    constexpr float current{ 2.0f };
    // A small node capacitance puts the stage's own knee at a few milliamps, so 2 A is deep in saturation
    const foc::PowerStageConfig stage{ 70e-9f, 10e-12f, 0.0f, 0.0f, 0.0f };
    const auto commanded = hal::DutyCycle::FromPercent(40);

    Register();
    inverterMock.TriggerPhaseCurrentsCallback(Currents(current, -current, 0.0f));
    const auto written = Output({ commanded, commanded, commanded });

    const auto uncorrected = foc::PowerStageLegVoltage(stage, foc::DutyFraction(commanded), current, busVoltage, period);
    const auto corrected = foc::PowerStageLegVoltage(stage, foc::DutyFraction(written.a), current, busVoltage, period);
    const auto target = foc::DutyFraction(commanded) * busVoltage;

    EXPECT_GT(std::abs(uncorrected - target), 30e-3f);
    EXPECT_NEAR(corrected, target, 1e-3f);
}
