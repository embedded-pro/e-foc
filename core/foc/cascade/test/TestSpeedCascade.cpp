#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/OnlineEstimatorsMock.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

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
        EXPECT_LE(duty.a.Value(), 100);
        EXPECT_LE(duty.b.Value(), 100);
        EXPECT_LE(duty.c.Value(), 100);
    }

    void ExpectCentredDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_NEAR(duty.a.Value(), 50, tolerance);
        EXPECT_NEAR(duty.b.Value(), 50, tolerance);
        EXPECT_NEAR(duty.c.Value(), 50, tolerance);
    }

    void ExpectSameDuty(const foc::PhasePwmDutyCycles& duty, const foc::PhasePwmDutyCycles& expected)
    {
        EXPECT_EQ(duty.a.Value(), expected.a.Value());
        EXPECT_EQ(duty.b.Value(), expected.b.Value());
        EXPECT_EQ(duty.c.Value(), expected.c.Value());
    }

    bool DutiesDiffer(const foc::PhasePwmDutyCycles& left, const foc::PhasePwmDutyCycles& right)
    {
        return left.a.Value() != right.a.Value() ||
               left.b.Value() != right.b.Value() ||
               left.c.Value() != right.c.Value();
    }

    void ExpectOffCentreDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_TRUE(DutiesDiffer(duty, foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } }));
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

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestSpeedCascade, consecutive_calls_update_speed_estimation)
{
    constexpr float step{ 0.00005f };

    focSpeed->SetCurrentTunings(UnsaturatedCurrentTunings());
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    foc::Radians start{ 0.0f };
    focSpeed->Calculate(ZeroCurrents(), start);
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians advanced{ step };
    focSpeed->Calculate(ZeroCurrents(), advanced);
    lowPriorityInterruptMock.TriggerHandler();

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

    EXPECT_CALL(mechanicalEstimator, Update(_, _, _));
    EXPECT_CALL(electricalEstimator, Update(_, _, _, _));

    foc::Radians position{ 0.0f };
    focSpeed->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
}
