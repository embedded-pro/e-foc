#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>

namespace
{
    constexpr uint32_t recordRateHz{ 10000 };
    constexpr uint32_t recordSamples{ 20000 };
    constexpr float cruiseSpeed{ 380.0f };
    constexpr float steppedSpeed{ 420.0f };
    constexpr float cruiseMs{ 250.0f };
    constexpr float windowMs{ 30.0f };

    struct DirectCurrent
    {
        float peak{ 0.0f };
        float peakQuadrature{ 0.0f };
    };

    DirectCurrent AfterSpeedStep(foc::CurrentAlgorithm algorithm, bool dutyAPeriodLate)
    {
        auto scenario = mil::Scenario{}.Plant("bench").CurrentLoop(algorithm).SpeedLoop(foc::SpeedAlgorithm::pid);
        if (dutyAPeriodLate)
            sil::DescribeBoardPwmUpdate(scenario.plant);
        scenario.Recorded(recordRateHz, recordSamples).Calibrated().BootsIn(state_machine::ControlMode::speed);

        mil::ModelInTheLoop mil{ scenario };
        EXPECT_TRUE(mil.Align());
        EXPECT_TRUE(mil.Enable());
        EXPECT_TRUE(mil.SetSpeed(cruiseSpeed));
        mil.Run(std::chrono::milliseconds{ static_cast<int>(cruiseMs) });
        EXPECT_TRUE(mil.SetSpeed(steppedSpeed));
        EXPECT_TRUE(mil.CaptureAfterSetpoint(windowMs));
        mil::ExpectNoDroppedSamples(mil);
        EXPECT_TRUE(mil.IsRunning());

        const auto setpointTick = mil.SetpointTick();
        EXPECT_TRUE(setpointTick.has_value());

        const auto until = *setpointTick + mil.TicksFromMilliseconds(windowMs);
        DirectCurrent result;
        uint32_t count{ 0 };
        for (const auto& sample : mil.Trace().Samples())
            if (sample.tick >= *setpointTick && sample.tick < until)
            {
                result.peak = std::max(result.peak, std::abs(sample.id));
                result.peakQuadrature = std::max(result.peakQuadrature, std::abs(sample.iq));
                ++count;
            }

        EXPECT_GT(count, 0u);
        return result;
    }

    void ExpectDirectCurrentKept(foc::CurrentAlgorithm algorithm, const char* label, bool dutyAPeriodLate, float peakLimit)
    {
        SCOPED_TRACE(label);
        const auto result = AfterSpeedStep(algorithm, dutyAPeriodLate);
        std::fprintf(stderr, "[METRIC] mil delay_compensation %s duty_late=%d peak_id_A=%.4f peak_iq_A=%.3f\n", label, dutyAPeriodLate ? 1 : 0, result.peak, result.peakQuadrature);

        EXPECT_GT(result.peakQuadrature, 1.0f);
        EXPECT_LT(result.peak, peakLimit);
    }
}

TEST(TestDelayCompensation, the_pid_current_law_keeps_the_direct_current_excursion_small_after_a_speed_step_at_400_rad_per_s_with_each_duty_a_period_late)
{
    ExpectDirectCurrentKept(foc::CurrentAlgorithm::pid, "pid", true, 0.54f);
}

TEST(TestDelayCompensation, the_decoupled_current_law_keeps_the_direct_current_excursion_small_after_a_speed_step_at_400_rad_per_s_with_each_duty_a_period_late)
{
    ExpectDirectCurrentKept(foc::CurrentAlgorithm::decoupledPid, "decoupled", true, 0.10f);
}

TEST(TestDelayCompensation, the_deadbeat_current_law_keeps_the_direct_current_excursion_small_after_a_speed_step_at_400_rad_per_s_with_each_duty_a_period_late)
{
    ExpectDirectCurrentKept(foc::CurrentAlgorithm::deadbeat, "deadbeat", true, 0.15f);
}

TEST(TestDelayCompensation, the_decoupled_current_law_keeps_the_direct_current_excursion_small_after_a_speed_step_at_400_rad_per_s_with_each_duty_at_once)
{
    ExpectDirectCurrentKept(foc::CurrentAlgorithm::decoupledPid, "decoupled", false, 0.10f);
}
