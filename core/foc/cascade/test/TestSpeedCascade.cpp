#include "core/foc/cascade/CurrentStream.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/OnlineEstimatorsMock.hpp"
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    float DutyPercent(hal::DutyCycle duty)
    {
        return 100.0f * foc::DutyFraction(duty);
    }

    constexpr uint32_t baseFrequencyValue = 20000;
    const hal::Hertz baseFrequency{ baseFrequencyValue };
    const hal::Hertz lowPriorityFrequency{ 2000 };
    constexpr float tolerance = 1.0f;

    foc::CurrentLoopTunings UnsaturatedCurrentTunings()
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = 628.3185f;
        return tunings;
    }

    foc::MotorModelParameters MotorParameters(std::size_t polePairs)
    {
        auto parameters = foc::MotorModelParameters{};
        parameters.resistance = foc::Ohm{ 1.0f };
        parameters.inductance = foc::MilliHenry{ 1.0f };
        parameters.fluxLinkage = foc::Weber{ 0.01f };
        parameters.busVoltage = foc::Volts{ 24.0f };
        parameters.samplingFrequency = baseFrequency;
        parameters.polePairs = polePairs;
        return parameters;
    }

    foc::MechanicalModelParameters MechanicalParameters()
    {
        auto parameters = foc::MechanicalModelParameters{};
        parameters.inertia = foc::NewtonMeterSecondSquared{ 0.001f };
        parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0001f };
        parameters.torqueConstant = foc::NewtonMeter{ 0.1f };
        parameters.maxCurrent = foc::Ampere{ 10.0f };
        parameters.samplingFrequency = lowPriorityFrequency;
        return parameters;
    }

    class TestSpeedCascade
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::StoreHandler));
            EXPECT_CALL(lowPriorityInterruptMock, Unregister()).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::ClearHandler));

            focSpeed.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focSpeed->Configure(MotorParameters(polePairs));
            focSpeed->ConfigureMechanics(MechanicalParameters());
            focSpeed->SetCurrentTunings(foc::CurrentLoopTunings{});
            focSpeed->SetSpeedTunings(foc::SpeedLoopTunings{});
        }

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        std::optional<foc::SpeedCascade> focSpeed;
        const std::size_t polePairs{ 7 };
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }

    void ExpectValidDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_LE(DutyPercent(duty.a), 100);
        EXPECT_LE(DutyPercent(duty.b), 100);
        EXPECT_LE(DutyPercent(duty.c), 100);
    }

    void ExpectCentredDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_NEAR(DutyPercent(duty.a), 50, tolerance);
        EXPECT_NEAR(DutyPercent(duty.b), 50, tolerance);
        EXPECT_NEAR(DutyPercent(duty.c), 50, tolerance);
    }

    void ExpectSameDuty(const foc::PhasePwmDutyCycles& duty, const foc::PhasePwmDutyCycles& expected)
    {
        EXPECT_EQ(DutyPercent(duty.a), DutyPercent(expected.a));
        EXPECT_EQ(DutyPercent(duty.b), DutyPercent(expected.b));
        EXPECT_EQ(DutyPercent(duty.c), DutyPercent(expected.c));
    }

    bool DutiesDiffer(const foc::PhasePwmDutyCycles& left, const foc::PhasePwmDutyCycles& right)
    {
        return DutyPercent(left.a) != DutyPercent(right.a) ||
               DutyPercent(left.b) != DutyPercent(right.b) ||
               DutyPercent(left.c) != DutyPercent(right.c);
    }

    void ExpectOffCentreDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_TRUE(DutiesDiffer(duty, foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) }));
    }

    struct SpeedCascadeUnderTest
    {
        SpeedCascadeUnderTest(std::size_t polePairs, const foc::SpeedLoopTunings& speedTunings)
        {
            EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
            EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));

            cascade.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency);
            cascade->Configure(MotorParameters(polePairs));
            cascade->ConfigureMechanics(MechanicalParameters());
            cascade->SetCurrentTunings(UnsaturatedCurrentTunings());
            cascade->SetSpeedTunings(speedTunings);
        }

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;
        std::optional<foc::SpeedCascade> cascade;
    };

    foc::PhasePwmDutyCycles DutyAfterConstantSpeed(uint32_t dutyUpdateDelayPeriods, std::size_t polePairs, float mechanicalStep, float angleOffset)
    {
        auto parameters = MotorParameters(polePairs);
        parameters.dutyUpdateDelayPeriods = dutyUpdateDelayPeriods;

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;
        EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
        EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));
        EXPECT_CALL(lowPriorityInterrupt, Trigger()).Times(testing::AnyNumber());

        foc::SpeedCascade cascade{ foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency };
        cascade.Configure(parameters);
        cascade.ConfigureMechanics(MechanicalParameters());
        cascade.SetCurrentTunings(UnsaturatedCurrentTunings());
        cascade.SetSpeedTunings(foc::SpeedLoopTunings{});
        cascade.Enable();
        cascade.SetPoint(foc::RadiansPerSecond{ 20.0f });

        foc::PhasePwmDutyCycles duty{};
        for (int sample = 0; sample != 400; ++sample)
        {
            foc::Radians position{ angleOffset + static_cast<float>(sample) * mechanicalStep };
            duty = cascade.Calculate(ZeroCurrents(), position);
            if (sample % static_cast<int>(baseFrequencyValue / lowPriorityFrequency.Value()) == 0)
                lowPriorityInterrupt.TriggerHandler();
        }
        return duty;
    }

    foc::PhasePwmDutyCycles FirstDutyAtStandstill(SpeedCascadeUnderTest& speedCascade, float stationaryAngle)
    {
        speedCascade.cascade->Enable();
        speedCascade.cascade->SetPoint(foc::RadiansPerSecond{ 0.0f });

        foc::Radians position{ stationaryAngle };
        speedCascade.cascade->Calculate(ZeroCurrents(), position);
        speedCascade.lowPriorityInterrupt.TriggerHandler();

        foc::Radians held{ stationaryAngle };
        return speedCascade.cascade->Calculate(ZeroCurrents(), held);
    }

    foc::PhasePwmDutyCycles FirstDutyAfterSpeedStep(SpeedCascadeUnderTest& speedCascade, float setPoint)
    {
        speedCascade.cascade->Enable();
        speedCascade.cascade->SetPoint(foc::RadiansPerSecond{ setPoint });

        foc::Radians position{ 0.0f };
        speedCascade.cascade->Calculate(ZeroCurrents(), position);
        speedCascade.lowPriorityInterrupt.TriggerHandler();

        foc::Radians secondSample{ 0.0f };
        return speedCascade.cascade->Calculate(ZeroCurrents(), secondSample);
    }
}

TEST_F(TestSpeedCascade, zero_speed_setpoint_produces_bounded_duty_cycles)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
}

TEST_F(TestSpeedCascade, duty_cycles_are_bounded_0_to_100)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 100.0f });

    foc::Radians position{ 0.5f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
}

TEST_F(TestSpeedCascade, set_pole_pairs)
{
    constexpr float mechanicalAngle{ 0.3f };

    for (std::size_t poles : { std::size_t{ 2 }, std::size_t{ 4 }, std::size_t{ 7 } })
    {
        SCOPED_TRACE(poles);

        SpeedCascadeUnderTest scaled{ poles, foc::SpeedLoopTunings{} };
        SpeedCascadeUnderTest reference{ 1, foc::SpeedLoopTunings{} };

        scaled.cascade->Enable();
        reference.cascade->Enable();
        scaled.cascade->SetPoint(foc::RadiansPerSecond{ 0.2f });
        reference.cascade->SetPoint(foc::RadiansPerSecond{ 0.2f });

        foc::Radians scaledStart{ 0.0f };
        foc::Radians referenceStart{ 0.0f };
        scaled.cascade->Calculate(ZeroCurrents(), scaledStart);
        reference.cascade->Calculate(ZeroCurrents(), referenceStart);
        scaled.lowPriorityInterrupt.TriggerHandler();
        reference.lowPriorityInterrupt.TriggerHandler();

        foc::Radians scaledPosition{ mechanicalAngle };
        foc::Radians referencePosition{ mechanicalAngle * static_cast<float>(poles) };

        ExpectSameDuty(scaled.cascade->Calculate(ZeroCurrents(), scaledPosition), reference.cascade->Calculate(ZeroCurrents(), referencePosition));
    }
}

TEST_F(TestSpeedCascade, enable_disable_cycle)
{
    focSpeed->SetCurrentTunings(UnsaturatedCurrentTunings());
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.2f });

    foc::Radians position{ 0.1f };
    focSpeed->Calculate(ZeroCurrents(), position);
    for (int cycle = 0; cycle != 20; ++cycle)
        lowPriorityInterruptMock.TriggerHandler();

    foc::Radians woundPosition{ 0.1f };
    auto wound = focSpeed->Calculate(ZeroCurrents(), woundPosition);

    focSpeed->Disable();
    focSpeed->Enable();

    SpeedCascadeUnderTest fresh{ polePairs, foc::SpeedLoopTunings{} };
    fresh.cascade->Enable();
    fresh.cascade->SetPoint(foc::RadiansPerSecond{ 0.2f });

    foc::Radians restarted{ 0.1f };
    foc::Radians freshPosition{ 0.1f };
    auto afterEnable = focSpeed->Calculate(ZeroCurrents(), restarted);
    auto firstOfFresh = fresh.cascade->Calculate(ZeroCurrents(), freshPosition);

    EXPECT_TRUE(DutiesDiffer(wound, afterEnable));
    ExpectSameDuty(afterEnable, firstOfFresh);
}

TEST_F(TestSpeedCascade, different_positions_produce_different_outputs)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    foc::Radians position1{ 0.0f };
    auto result1 = focSpeed->Calculate(ZeroCurrents(), position1);

    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position2{ 1.0f };
    auto result2 = focSpeed->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (DutyPercent(result1.a) != DutyPercent(result2.a)) ||
                        (DutyPercent(result1.b) != DutyPercent(result2.b)) ||
                        (DutyPercent(result1.c) != DutyPercent(result2.c));

    EXPECT_TRUE(anyDifferent);
}

// A duty one period later than another acts one electrical step further on, so the voltage must be
// rotated one more step: (n = 1, sampled at θ) lands where (n = 0, sampled one step later) does.
TEST_F(TestSpeedCascade, the_inverse_park_angle_is_advanced_by_the_duty_delay_and_the_hold_at_a_positive_speed)
{
    constexpr float step = 0.005f;

    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, step, 0.0f), DutyAfterConstantSpeed(0, polePairs, step, step));
}

TEST_F(TestSpeedCascade, the_inverse_park_angle_is_advanced_by_the_duty_delay_and_the_hold_at_a_negative_speed)
{
    constexpr float step = -0.005f;

    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, step, 0.0f), DutyAfterConstantSpeed(0, polePairs, step, step));
}

TEST_F(TestSpeedCascade, the_advance_moves_the_voltage_once_the_rotor_turns)
{
    constexpr float step = 0.005f;

    EXPECT_GT(std::abs(DutyPercent(DutyAfterConstantSpeed(1, polePairs, step, 0.0f).a) - DutyPercent(DutyAfterConstantSpeed(0, polePairs, step, 0.0f).a)), 0.1f);
}

TEST_F(TestSpeedCascade, a_duty_that_acts_later_leaves_the_voltage_at_standstill_unchanged)
{
    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, 0.0f, 0.3f), DutyAfterConstantSpeed(0, polePairs, 0.0f, 0.3f));
}

TEST_F(TestSpeedCascade, consecutive_calls_update_speed_estimation)
{
    constexpr float step{ 0.00005f };

    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());

    focSpeed->SetCurrentTunings(UnsaturatedCurrentTunings());
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    for (const auto angle : { 0.0f, step })
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians held{ angle };
            focSpeed->Calculate(ZeroCurrents(), held);
        }

        lowPriorityInterruptMock.TriggerHandler();
    }

    foc::Radians moved{ step };
    auto movingRotor = focSpeed->Calculate(ZeroCurrents(), moved);

    SpeedCascadeUnderTest stationary{ polePairs, foc::SpeedLoopTunings{} };
    stationary.cascade->Enable();
    stationary.cascade->SetPoint(foc::RadiansPerSecond{ 0.0f });

    for (int cycle = 0; cycle != 2; ++cycle)
    {
        foc::Radians held{ 0.0f };
        stationary.cascade->Calculate(ZeroCurrents(), held);
        stationary.lowPriorityInterrupt.TriggerHandler();
    }

    foc::Radians held{ 0.0f };
    auto stationaryRotor = stationary.cascade->Calculate(ZeroCurrents(), held);

    ExpectCentredDuty(stationaryRotor);
    EXPECT_TRUE(DutiesDiffer(movingRotor, stationaryRotor));
}

TEST_F(TestSpeedCascade, set_speed_tunings)
{
    auto slowTunings = foc::SpeedLoopTunings{};
    slowTunings.bandwidth = foc::SpeedLoopTunings{}.bandwidth / 10.0f;

    SpeedCascadeUnderTest slowLoop{ polePairs, slowTunings };
    SpeedCascadeUnderTest fastLoop{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest atSetPoint{ polePairs, foc::SpeedLoopTunings{} };

    auto slowResult = FirstDutyAfterSpeedStep(slowLoop, 0.2f);
    auto fastResult = FirstDutyAfterSpeedStep(fastLoop, 0.2f);

    EXPECT_TRUE(DutiesDiffer(slowResult, fastResult));
    ExpectCentredDuty(FirstDutyAfterSpeedStep(atSetPoint, 0.0f));
}

TEST_F(TestSpeedCascade, prescaler_triggers_low_priority_at_correct_rate)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(1);

    for (uint32_t i = 0; i < prescaler; ++i)
    {
        foc::Radians position{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), position);
    }
}

TEST_F(TestSpeedCascade, second_prescaler_cycle_triggers_again)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(2);

    for (uint32_t i = 0; i < prescaler * 2; ++i)
    {
        foc::Radians position{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), position);
    }
}

TEST_F(TestSpeedCascade, the_cascade_supplies_the_current_envelope_and_rate_the_caller_cannot_know)
{
    auto withoutLimits = MechanicalParameters();
    withoutLimits.maxCurrent = foc::Ampere{ 0.0f };
    withoutLimits.samplingFrequency = hal::Hertz{ 0 };

    focSpeed->ConfigureMechanics(withoutLimits);

    EXPECT_EQ(focSpeed->SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::ok);
    EXPECT_EQ(focSpeed->ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::lqi);
}

TEST_F(TestSpeedCascade, a_speed_loop_configured_without_limits_still_drives_the_output)
{
    auto withoutLimits = MechanicalParameters();
    withoutLimits.maxCurrent = foc::Ampere{ 0.0f };
    withoutLimits.samplingFrequency = hal::Hertz{ 0 };

    focSpeed->ConfigureMechanics(withoutLimits);
    focSpeed->SetSpeedTunings(foc::SpeedLoopTunings{});
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 100.0f });

    foc::Radians position{ 0.0f };
    lowPriorityInterruptMock.TriggerHandler();
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
    ExpectOffCentreDuty(result);
}

TEST(TestSpeedCascadeLifetime, destruction_unregisters_the_outer_loop_handler_so_a_queued_trigger_is_inert)
{
    StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;

    {
        InSequence sequence;
        EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
        EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));
    }

    {
        foc::SpeedCascade cascade{ foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency };
        cascade.Configure(MotorParameters(7));
        cascade.ConfigureMechanics(MechanicalParameters());
        cascade.SetCurrentTunings(foc::CurrentLoopTunings{});
        cascade.SetSpeedTunings(foc::SpeedLoopTunings{});
        cascade.Enable();
    }

    lowPriorityInterrupt.TriggerHandler();

    EXPECT_FALSE(lowPriorityInterrupt.HasHandler());
}

TEST_F(TestSpeedCascade, the_speed_estimate_does_not_spike_on_the_first_sample_after_enable)
{
    SpeedCascadeUnderTest atOrigin{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest awayFromOrigin{ polePairs, foc::SpeedLoopTunings{} };

    auto atOriginDuty = FirstDutyAtStandstill(atOrigin, 0.0f);
    auto awayFromOriginDuty = FirstDutyAtStandstill(awayFromOrigin, std::numbers::pi_v<float> - 0.01f);

    ExpectSameDuty(awayFromOriginDuty, atOriginDuty);
}

TEST_F(TestSpeedCascade, outer_loop_frequency_equals_configured_low_priority_frequency)
{
    EXPECT_EQ(focSpeed->OuterLoopFrequency().Value(), lowPriorityFrequency.Value());
}

TEST_F(TestSpeedCascade, registered_online_estimators_are_fed_from_the_outer_loop)
{
    testing::StrictMock<foc::OnlineMechanicalEstimatorMock> mechanicalEstimator;
    testing::StrictMock<foc::OnlineElectricalEstimatorMock> electricalEstimator;

    focSpeed->SetOnlineMechanicalEstimator(mechanicalEstimator);
    focSpeed->SetOnlineElectricalEstimator(electricalEstimator);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    EXPECT_CALL(mechanicalEstimator, Update(_));
    EXPECT_CALL(electricalEstimator, Update(_));

    foc::Radians position{ 0.0f };
    focSpeed->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
}

TEST_F(TestSpeedCascade, the_electrical_window_carries_the_direct_axis_reference_it_ran_with)
{
    testing::StrictMock<foc::OnlineElectricalEstimatorMock> electricalEstimator;
    const auto reference = [](float value)
    {
        return testing::ResultOf([](const foc::ElectricalWindow& window)
            {
                return window.directCurrentReference.Value();
            },
            testing::FloatEq(value));
    };

    focSpeed->SetOnlineElectricalEstimator(electricalEstimator);
    focSpeed->Enable();

    testing::InSequence sequence;
    EXPECT_CALL(electricalEstimator, Update(reference(0.0f)));
    EXPECT_CALL(electricalEstimator, Update(reference(0.25f)));

    foc::Radians position{ 0.0f };
    for (int window = 0; window != 2; ++window)
    {
        focSpeed->Calculate(ZeroCurrents(), position);
        lowPriorityInterruptMock.TriggerHandler();
    }
}

TEST_F(TestSpeedCascade, enable_speed_command_drives_the_motor_at_commanded_speed)
{
    focSpeed->EnableSpeedCommand();
    focSpeed->CommandSpeed(foc::RadiansPerSecond{ 50.0f });

    foc::Radians position{ 0.0f };
    focSpeed->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians second{ 0.0f };
    ExpectValidDuty(focSpeed->Calculate(ZeroCurrents(), second));
}

TEST_F(TestSpeedCascade, disable_speed_command_produces_bounded_duty)
{
    focSpeed->EnableSpeedCommand();
    focSpeed->CommandSpeed(foc::RadiansPerSecond{ 50.0f });

    foc::Radians position{ 0.0f };
    focSpeed->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();

    focSpeed->DisableSpeedCommand();

    foc::Radians stopped{ 0.0f };
    ExpectValidDuty(focSpeed->Calculate(ZeroCurrents(), stopped));
}

TEST_F(TestSpeedCascade, speed_command_frequency_equals_outer_loop_frequency)
{
    EXPECT_EQ(focSpeed->SpeedCommandFrequency().Value(), focSpeed->OuterLoopFrequency().Value());
}

TEST_F(TestSpeedCascade, the_observed_motion_carries_the_window_speed_and_the_reference)
{
    constexpr float step{ 0.001f };
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());

    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });

    for (const auto angle : { 0.0f, step })
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians held{ angle };
            focSpeed->Calculate(ZeroCurrents(), held);
        }

        lowPriorityInterruptMock.TriggerHandler();
    }

    const auto observation = focSpeed->ObserveMotion();
    EXPECT_NEAR(observation.measuredSpeed.Value(), step * static_cast<float>(lowPriorityFrequency.Value()), 1e-3f);
    EXPECT_NEAR(observation.demandedSpeed.Value(), 12.0f, 1e-6f);
    EXPECT_NEAR(observation.positionError.Value(), 0.0f, 1e-6f);
}

TEST_F(TestSpeedCascade, a_disabled_cascade_observes_no_motion)
{
    constexpr float step{ 0.001f };
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());

    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });

    for (const auto angle : { 0.0f, step })
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians held{ angle };
            focSpeed->Calculate(ZeroCurrents(), held);
        }

        lowPriorityInterruptMock.TriggerHandler();
    }

    focSpeed->Disable();

    const auto observation = focSpeed->ObserveMotion();
    EXPECT_NEAR(observation.measuredSpeed.Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(observation.measuredTorqueCurrent.Value(), 0.0f, 1e-6f);
}

TEST_F(TestSpeedCascade, a_current_command_bypasses_the_speed_loop)
{
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    focSpeed->SetPoint(foc::RadiansPerSecond{ 50.0f });
    focSpeed->EnableCurrentCommand();
    focSpeed->CommandCurrent(foc::Ampere{ 0.0f });

    for (uint32_t tick = 0; tick != prescaler; ++tick)
    {
        foc::Radians held{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), held);
    }
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position{ 0.0f };
    ExpectCentredDuty(focSpeed->Calculate(ZeroCurrents(), position));
}

TEST_F(TestSpeedCascade, a_current_command_reports_the_mean_q_axis_voltage_it_applies)
{
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    focSpeed->EnableCurrentCommand();
    focSpeed->CommandCurrent(foc::Ampere{ 2.0f });

    for (int window = 0; window != 3; ++window)
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians held{ 0.0f };
            focSpeed->Calculate(ZeroCurrents(), held);
        }
        lowPriorityInterruptMock.TriggerHandler();
    }

    EXPECT_GT(focSpeed->QuadratureVoltage().Value(), 0.0f);

    focSpeed->DisableCurrentCommand();
    EXPECT_NEAR(focSpeed->QuadratureVoltage().Value(), 0.0f, 1e-6f);
}

TEST_F(TestSpeedCascade, outside_a_current_command_no_q_axis_voltage_is_kept)
{
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    focSpeed->SetPoint(foc::RadiansPerSecond{ 50.0f });
    focSpeed->Enable();

    for (int window = 0; window != 3; ++window)
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians held{ 0.0f };
            focSpeed->Calculate(ZeroCurrents(), held);
        }
        lowPriorityInterruptMock.TriggerHandler();
    }

    EXPECT_NEAR(focSpeed->QuadratureVoltage().Value(), 0.0f, 1e-6f);
}

namespace
{
    std::array<int8_t, 48> CoggingWave()
    {
        std::array<int8_t, 48> torque{};
        for (std::size_t bin = 0; bin != torque.size(); ++bin)
            torque[bin] = static_cast<int8_t>(std::lround(127.0f * std::sin(6.0f * (static_cast<float>(bin) + 0.5f) * 2.0f * std::numbers::pi_v<float> / 48.0f)));
        return torque;
    }
}

TEST_F(TestSpeedCascade, a_loaded_cogging_map_asks_the_winding_for_the_cogging_torque_at_standstill)
{
    SpeedCascadeUnderTest withoutMap{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest withMap{ polePairs, foc::SpeedLoopTunings{} };
    const auto torque = CoggingWave();
    withMap.cascade->LoadCoggingMap(infra::MakeRange(torque), 0.05f / 127.0f, foc::NewtonMeter{ 0.1f });

    ExpectCentredDuty(FirstDutyAtStandstill(withoutMap, 0.03f));
    ExpectOffCentreDuty(FirstDutyAtStandstill(withMap, 0.03f));
}

TEST_F(TestSpeedCascade, a_cleared_cogging_map_no_longer_drives_the_winding)
{
    SpeedCascadeUnderTest speedCascade{ polePairs, foc::SpeedLoopTunings{} };
    const auto torque = CoggingWave();
    speedCascade.cascade->LoadCoggingMap(infra::MakeRange(torque), 0.05f / 127.0f, foc::NewtonMeter{ 0.1f });

    speedCascade.cascade->ClearCoggingMap();

    ExpectCentredDuty(FirstDutyAtStandstill(speedCascade, 0.03f));
}

TEST_F(TestSpeedCascade, an_attached_stream_receives_the_measured_currents_and_the_mechanical_angle)
{
    foc::CurrentStream stream;
    focSpeed->SetCurrentStream(stream);
    focSpeed->Enable();
    stream.Start(1);

    foc::Radians position{ 0.25f };
    const float electricalAngle = 0.25f * static_cast<float>(polePairs);
    focSpeed->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), std::cos(electricalAngle), 1e-2f);
    EXPECT_NEAR(sample->meanIq.Value(), -std::sin(electricalAngle), 1e-2f);
    EXPECT_NEAR(sample->position.Value(), 0.25f, 1e-6f);
}

TEST_F(TestSpeedCascade, enabling_restarts_the_stream_window)
{
    foc::CurrentStream stream;
    focSpeed->SetCurrentStream(stream);
    focSpeed->Enable();
    stream.Start(2);

    foc::Radians position{ 0.0f };
    focSpeed->Calculate({ foc::Ampere{ 3.0f }, foc::Ampere{ -1.5f }, foc::Ampere{ -1.5f } }, position);
    focSpeed->Disable();
    focSpeed->Enable();
    focSpeed->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);
    EXPECT_FALSE(stream.Take().has_value());
    focSpeed->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 1.0f, 1e-2f);
}

TEST_F(TestSpeedCascade, the_observed_demand_is_the_current_the_speed_loop_commands)
{
    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    focSpeed->EnableCurrentCommand();
    focSpeed->CommandCurrent(foc::Ampere{ 1.5f });

    for (uint32_t tick = 0; tick != prescaler; ++tick)
    {
        foc::Radians held{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), held);
    }
    lowPriorityInterruptMock.TriggerHandler();

    EXPECT_NEAR(focSpeed->ObserveMotion().demandedTorqueCurrent.Value(), 1.5f, 1e-6f);
}

namespace
{
    class TestSpeedCascadeRamp
        : public TestSpeedCascade
    {
    public:
        void SetUp() override
        {
            TestSpeedCascade::SetUp();
            EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(testing::AnyNumber());
        }

        void Ticks(uint32_t count)
        {
            const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();

            for (uint32_t outer = 0; outer != count; ++outer)
            {
                for (uint32_t tick = 0; tick != prescaler; ++tick)
                {
                    foc::Radians held{ 0.0f };
                    focSpeed->Calculate(ZeroCurrents(), held);
                }

                lowPriorityInterruptMock.TriggerHandler();
            }
        }

        float Reference() const
        {
            return focSpeed->ObserveMotion().demandedSpeed.Value();
        }

        void Ramp(float acceleration, float deceleration)
        {
            focSpeed->RampSpeed(foc::RadiansPerSecondSquared{ acceleration }, foc::RadiansPerSecondSquared{ deceleration });
        }
    };
}

TEST_F(TestSpeedCascadeRamp, without_a_ramp_the_setpoint_is_the_reference_at_once)
{
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });

    EXPECT_NEAR(Reference(), 12.0f, 1e-6f);
}

TEST_F(TestSpeedCascadeRamp, the_reference_grows_by_the_acceleration_each_outer_loop_period)
{
    Ramp(1000.0f, 1000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    EXPECT_NEAR(Reference(), 0.0f, 1e-6f);

    Ticks(1);
    EXPECT_NEAR(Reference(), 0.5f, 1e-5f);

    Ticks(3);
    EXPECT_NEAR(Reference(), 2.0f, 1e-5f);

    Ticks(100);
    EXPECT_NEAR(Reference(), 12.0f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, the_reference_shrinks_by_the_deceleration)
{
    Ramp(1000.0f, 2000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    Ticks(100);

    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });
    Ticks(1);

    EXPECT_NEAR(Reference(), 11.0f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, a_reversal_decelerates_to_zero_before_it_accelerates)
{
    Ramp(1000.0f, 2000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 1.5f });
    Ticks(100);

    focSpeed->SetPoint(foc::RadiansPerSecond{ -2.0f });
    Ticks(1);
    EXPECT_NEAR(Reference(), 0.5f, 1e-5f);

    Ticks(1);
    EXPECT_NEAR(Reference(), 0.0f, 1e-5f);

    Ticks(1);
    EXPECT_NEAR(Reference(), -0.5f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, a_reverse_setpoint_accelerates_at_the_acceleration)
{
    Ramp(1000.0f, 2000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ -12.0f });

    Ticks(2);

    EXPECT_NEAR(Reference(), -1.0f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, a_direction_without_a_rate_is_not_limited)
{
    Ramp(1000.0f, 0.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    Ticks(100);

    focSpeed->SetPoint(foc::RadiansPerSecond{ 2.0f });
    Ticks(1);

    EXPECT_NEAR(Reference(), 2.0f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, every_enable_starts_the_ramp_from_zero)
{
    Ramp(1000.0f, 1000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    Ticks(100);
    focSpeed->Disable();

    focSpeed->Enable();
    EXPECT_NEAR(Reference(), 0.0f, 1e-6f);

    Ticks(1);
    EXPECT_NEAR(Reference(), 0.5f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, a_ramp_set_while_enabled_is_in_force_from_the_next_period)
{
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    Ticks(1);

    Ramp(1000.0f, 1000.0f);
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });
    Ticks(1);

    EXPECT_NEAR(Reference(), 11.5f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, turning_the_ramp_off_hands_the_setpoint_to_the_loop_at_once)
{
    Ramp(1000.0f, 1000.0f);
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 12.0f });
    Ticks(1);

    Ramp(0.0f, 0.0f);

    EXPECT_NEAR(Reference(), 12.0f, 1e-6f);
}

TEST_F(TestSpeedCascadeRamp, an_operator_enable_after_a_speed_command_is_ramped_again)
{
    Ramp(1000.0f, 1000.0f);
    focSpeed->EnableSpeedCommand();
    focSpeed->CommandSpeed(foc::RadiansPerSecond{ 12.0f });
    focSpeed->DisableSpeedCommand();

    focSpeed->Enable();
    Ticks(1);

    EXPECT_NEAR(Reference(), 0.5f, 1e-5f);
}

TEST_F(TestSpeedCascadeRamp, a_calibration_speed_command_is_not_ramped)
{
    Ramp(1000.0f, 1000.0f);
    focSpeed->EnableSpeedCommand();
    focSpeed->CommandSpeed(foc::RadiansPerSecond{ 12.0f });

    EXPECT_NEAR(Reference(), 12.0f, 1e-6f);

    Ticks(1);
    EXPECT_NEAR(Reference(), 12.0f, 1e-6f);
}

TEST_F(TestSpeedCascadeRamp, the_demanded_torque_current_is_the_one_the_current_limit_lets_through)
{
    focSpeed->LimitCurrent(foc::Ampere{ 1.0f });
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 500.0f });
    Ticks(20);

    EXPECT_NEAR(focSpeed->ObserveMotion().demandedTorqueCurrent.Value(), 1.0f, 1e-6f);

    focSpeed->LimitCurrent(foc::Ampere{ 0.0f });
    Ticks(1);

    EXPECT_NEAR(focSpeed->ObserveMotion().demandedTorqueCurrent.Value(), 0.0f, 1e-6f);
}

TEST_F(TestSpeedCascadeRamp, a_current_limit_clamps_the_reference_the_speed_law_hands_the_inner_loop)
{
    focSpeed->LimitCurrent(foc::Ampere{ 1.0f });
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 500.0f });
    Ticks(20);
    foc::Radians held{ 0.0f };
    const auto limited = focSpeed->Calculate(ZeroCurrents(), held);

    testing::StrictMock<foc::LowPriorityInterruptMock> referenceInterrupt;
    EXPECT_CALL(referenceInterrupt, Register(_)).WillOnce(Invoke(&referenceInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
    EXPECT_CALL(referenceInterrupt, Unregister()).WillOnce(Invoke(&referenceInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));
    EXPECT_CALL(referenceInterrupt, Trigger()).Times(testing::AnyNumber());
    foc::SpeedCascade reference{ foc::Ampere{ 1.0f }, baseFrequency, referenceInterrupt, lowPriorityFrequency };
    reference.Configure(MotorParameters(polePairs));
    auto mechanics = MechanicalParameters();
    mechanics.maxCurrent = foc::Ampere{ 1.0f };
    reference.ConfigureMechanics(mechanics);
    reference.SetCurrentTunings(foc::CurrentLoopTunings{});
    reference.SetSpeedTunings(foc::SpeedLoopTunings{});
    reference.Enable();
    reference.SetPoint(foc::RadiansPerSecond{ 500.0f });
    const uint32_t prescaler = baseFrequencyValue / lowPriorityFrequency.Value();
    for (uint32_t outer = 0; outer != 20; ++outer)
    {
        for (uint32_t tick = 0; tick != prescaler; ++tick)
        {
            foc::Radians position{ 0.0f };
            reference.Calculate(ZeroCurrents(), position);
        }
        referenceInterrupt.TriggerHandler();
    }
    foc::Radians referencePosition{ 0.0f };
    const auto expected = reference.Calculate(ZeroCurrents(), referencePosition);

    ExpectSameDuty(limited, expected);
}

TEST_F(TestSpeedCascade, a_bus_at_half_the_configured_voltage_doubles_the_inner_loop_duty)
{
    SpeedCascadeUnderTest nominal{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest halvedBus{ polePairs, foc::SpeedLoopTunings{} };
    halvedBus.cascade->TrackBusVoltage(foc::Volts{ 12.0f });

    const auto expected = FirstDutyAfterSpeedStep(nominal, 5.0f);
    const auto halved = FirstDutyAfterSpeedStep(halvedBus, 5.0f);

    ExpectOffCentreDuty(expected);
    EXPECT_NEAR(DutyPercent(halved.a) - 50.0f, 2.0f * (DutyPercent(expected.a) - 50.0f), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.b) - 50.0f, 2.0f * (DutyPercent(expected.b) - 50.0f), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.c) - 50.0f, 2.0f * (DutyPercent(expected.c) - 50.0f), 0.1f);
}

namespace
{
    constexpr float frictionTorque{ 0.5f };
    constexpr float justBeyondTheRamp{ 1.25f * foc::FrictionFeedForward::rampWidth };

    float QuadratureDrive(const foc::PhasePwmDutyCycles& duty)
    {
        return DutyPercent(duty.b) - DutyPercent(duty.c);
    }

    foc::SpeedLoopTunings SlowSpeedLoop()
    {
        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = 1.0f;
        return tunings;
    }

    float DriveAddedByFriction(std::size_t polePairs, float setPoint, float ceiling = 10.0f)
    {
        SpeedCascadeUnderTest withFriction{ polePairs, SlowSpeedLoop() };
        SpeedCascadeUnderTest withoutFriction{ polePairs, SlowSpeedLoop() };
        withFriction.cascade->LoadFrictionFeedForward(foc::NewtonMeter{ frictionTorque }, foc::NewtonMeter{ 0.1f });
        withFriction.cascade->LimitCurrent(foc::Ampere{ ceiling });
        withoutFriction.cascade->LimitCurrent(foc::Ampere{ ceiling });

        return QuadratureDrive(FirstDutyAfterSpeedStep(withFriction, setPoint)) - QuadratureDrive(FirstDutyAfterSpeedStep(withoutFriction, setPoint));
    }
}

TEST_F(TestSpeedCascade, a_forward_speed_reference_adds_the_friction_current_to_the_quadrature_reference)
{
    EXPECT_GT(DriveAddedByFriction(polePairs, justBeyondTheRamp), 1.0f);
}

TEST_F(TestSpeedCascade, a_reverse_speed_reference_subtracts_the_friction_current)
{
    EXPECT_LT(DriveAddedByFriction(polePairs, -justBeyondTheRamp), -1.0f);
}

TEST_F(TestSpeedCascade, the_friction_current_is_the_same_either_way_round)
{
    EXPECT_NEAR(DriveAddedByFriction(polePairs, justBeyondTheRamp), -DriveAddedByFriction(polePairs, -justBeyondTheRamp), 1e-3f);
}

TEST_F(TestSpeedCascade, a_zero_speed_reference_adds_no_friction_current)
{
    EXPECT_NEAR(DriveAddedByFriction(polePairs, 0.0f), 0.0f, 1e-4f);
}

TEST_F(TestSpeedCascade, a_reference_inside_the_ramp_adds_a_proportional_share_of_the_friction_current)
{
    const auto full = DriveAddedByFriction(polePairs, justBeyondTheRamp);

    EXPECT_NEAR(DriveAddedByFriction(polePairs, 0.5f * foc::FrictionFeedForward::rampWidth), 0.5f * full, 0.02f * full);
}

TEST_F(TestSpeedCascade, the_sum_of_speed_loop_and_friction_current_is_held_within_the_current_limit)
{
    EXPECT_NEAR(DriveAddedByFriction(polePairs, 500.0f, 1.0f), 0.0f, 1e-4f);
}

TEST_F(TestSpeedCascade, a_cleared_friction_adds_no_current)
{
    SpeedCascadeUnderTest cleared{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest reference{ polePairs, foc::SpeedLoopTunings{} };
    cleared.cascade->LoadFrictionFeedForward(foc::NewtonMeter{ frictionTorque }, foc::NewtonMeter{ 0.1f });
    cleared.cascade->ClearFrictionFeedForward();

    ExpectSameDuty(FirstDutyAfterSpeedStep(cleared, 50.0f), FirstDutyAfterSpeedStep(reference, 50.0f));
}

TEST_F(TestSpeedCascade, friction_is_not_added_to_the_current_a_current_command_asks_for)
{
    SpeedCascadeUnderTest commanded{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest reference{ polePairs, foc::SpeedLoopTunings{} };
    commanded.cascade->LoadFrictionFeedForward(foc::NewtonMeter{ frictionTorque }, foc::NewtonMeter{ 0.1f });
    auto firstDuty = [](SpeedCascadeUnderTest& under)
    {
        under.cascade->EnableCurrentCommand();
        under.cascade->CommandCurrent(foc::Ampere{ 1.0f });
        foc::Radians position{ 0.0f };
        under.cascade->Calculate(ZeroCurrents(), position);
        under.lowPriorityInterrupt.TriggerHandler();
        return under.cascade->Calculate(ZeroCurrents(), position);
    };

    ExpectSameDuty(firstDuty(commanded), firstDuty(reference));
}

TEST_F(TestSpeedCascade, a_friction_beyond_the_current_envelope_adds_no_current)
{
    SpeedCascadeUnderTest implausible{ polePairs, foc::SpeedLoopTunings{} };
    SpeedCascadeUnderTest reference{ polePairs, foc::SpeedLoopTunings{} };
    implausible.cascade->LoadFrictionFeedForward(foc::NewtonMeter{ 1.5f }, foc::NewtonMeter{ 0.1f });

    ExpectSameDuty(FirstDutyAfterSpeedStep(implausible, 50.0f), FirstDutyAfterSpeedStep(reference, 50.0f));
}
