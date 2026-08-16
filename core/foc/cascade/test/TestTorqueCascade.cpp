#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "numerical/math/Tolerance.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1.0f;

    foc::MotorModelParameters MotorParameters(std::size_t polePairs)
    {
        auto parameters = foc::MotorModelParameters{};
        parameters.resistance = foc::Ohm{ 1.0f };
        parameters.inductance = foc::MilliHenry{ 1.0f };
        parameters.fluxLinkage = foc::Weber{ 0.01f };
        parameters.busVoltage = foc::Volts{ 24.0f };
        parameters.samplingFrequency = hal::Hertz{ 20000 };
        parameters.polePairs = polePairs;
        return parameters;
    }

    class TestTorqueCascade
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            focTorque.emplace();
            focTorque->Configure(MotorParameters(polePairs));
            focTorque->Enable();
        }

        std::optional<foc::TorqueCascade> focTorque;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestTorqueCascade, zero_setpoint_and_zero_currents_gives_50_percent_duty_cycles)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestTorqueCascade, duty_cycles_are_bounded_0_to_100)
{
    focTorque->SetPoint({ foc::Ampere{ 10.0f }, foc::Ampere{ 10.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.5f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

// theta_e = theta_m * pole_pairs: p pole pairs at a mechanical angle must land on the same
// electrical angle - and therefore the same duty cycles - as 1 pole pair at p times that angle.
TEST_F(TestTorqueCascade, electrical_angle_scales_with_pole_pairs)
{
    const foc::PhaseCurrents currents{ foc::Ampere{ 0.4f }, foc::Ampere{ -0.1f }, foc::Ampere{ -0.3f } };
    const float mechanicalAngle = 0.3f;

    for (std::size_t poles : { std::size_t{ 1 }, std::size_t{ 2 }, std::size_t{ 4 }, std::size_t{ 7 } })
    {
        foc::TorqueCascade scaled;
        scaled.Configure(MotorParameters(poles));
        scaled.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });
        scaled.Enable();

        foc::TorqueCascade reference;
        reference.Configure(MotorParameters(1));
        reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });
        reference.Enable();

        foc::Radians scaledPosition{ mechanicalAngle };
        foc::Radians referencePosition{ mechanicalAngle * static_cast<float>(poles) };

        auto scaledResult = scaled.Calculate(currents, scaledPosition);
        auto referenceResult = reference.Calculate(currents, referencePosition);

        EXPECT_EQ(scaledResult.a.Value(), referenceResult.a.Value()) << "pole pairs " << poles;
        EXPECT_EQ(scaledResult.b.Value(), referenceResult.b.Value()) << "pole pairs " << poles;
        EXPECT_EQ(scaledResult.c.Value(), referenceResult.c.Value()) << "pole pairs " << poles;
    }
}

TEST_F(TestTorqueCascade, enable_disable_cycle)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    focTorque->Disable();
    focTorque->Enable();

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestTorqueCascade, different_positions_produce_different_outputs)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position1{ 0.0f };
    auto result1 = focTorque->Calculate(ZeroCurrents(), position1);

    foc::Radians position2{ 1.0f };
    auto result2 = focTorque->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestTorqueCascade, non_zero_current_setpoint_produces_non_centered_output)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    bool anyNon50 = (result.a.Value() != 50) || (result.b.Value() != 50) || (result.c.Value() != 50);
    EXPECT_TRUE(anyNon50);
}

TEST_F(TestTorqueCascade, pid_is_the_default_current_algorithm)
{
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestTorqueCascade, algorithm_selection_is_rejected_while_enabled)
{
    EXPECT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::busy);
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestTorqueCascade, algorithm_selection_is_accepted_while_disabled)
{
    focTorque->Disable();

    EXPECT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(TestTorqueCascade, selected_algorithm_drives_the_output)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    bool anyNon50 = (result.a.Value() != 50) || (result.b.Value() != 50) || (result.c.Value() != 50);
    EXPECT_TRUE(anyNon50);
}

TEST_F(TestTorqueCascade, a_stationary_rotor_produces_no_back_emf_feedforward)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    foc::Radians position{ 0.0f };
    for (int sample = 0; sample != 100; ++sample)
        focTorque->Calculate(ZeroCurrents(), position);

    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestTorqueCascade, a_spinning_rotor_drives_the_back_emf_feedforward)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    // Torque mode has no outer loop, so the cascade must derive speed from the angle it is handed
    constexpr float mechanicalStepPerSample = 0.01f;
    float angle = 0.0f;
    foc::PhasePwmDutyCycles result{ hal::Percent{ 0 }, hal::Percent{ 0 }, hal::Percent{ 0 } };

    for (int sample = 0; sample != 200; ++sample)
    {
        foc::Radians position{ angle };
        result = focTorque->Calculate(ZeroCurrents(), position);
        angle = foc::detail::PositionWithWrapAround(angle + mechanicalStepPerSample);
    }

    const bool anyOffCentre = std::abs(result.a.Value() - 50) > tolerance ||
                              std::abs(result.b.Value() - 50) > tolerance ||
                              std::abs(result.c.Value() - 50) > tolerance;

    EXPECT_TRUE(anyOffCentre);
}

TEST_F(TestTorqueCascade, the_speed_estimate_does_not_spike_on_the_first_sample_after_enable)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    // A rotor parked far from zero must not read as a huge step on the first sample
    foc::Radians position{ 3.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestTorqueCascade, the_sliding_mode_algorithm_drives_the_inverter)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::slidingMode), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 2.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_TRUE(result.a.Value() != 50 || result.b.Value() != 50 || result.c.Value() != 50);
}
