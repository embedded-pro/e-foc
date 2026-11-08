#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr float referenceSpeed{ 100.0f };
    constexpr int samples{ 500 };

    mil::Scenario QuantisedEncoderBench(foc::SpeedAlgorithm algorithm)
    {
        mil::Scenario scenario;
        scenario.Plant("bench").SpeedLoop(algorithm);
        scenario.Calibrated().BootsIn(state_machine::ControlMode::speed);
        return scenario;
    }

    float DemandedCurrentRipple(foc::SpeedAlgorithm algorithm)
    {
        mil::ModelInTheLoop mil{ QuantisedEncoderBench(algorithm) };
        EXPECT_TRUE(mil.Align());
        EXPECT_TRUE(mil.SetSpeed(referenceSpeed));
        EXPECT_TRUE(mil.Enable());
        mil.Run(1500ms);

        std::vector<float> demanded;
        for (int sample = 0; sample != samples; ++sample)
        {
            mil.Run(1ms);
            demanded.push_back(mil.ControlMode().ActiveStateMachine().ObserveMotion().demandedTorqueCurrent.Value());
        }

        double mean{ 0.0 };
        for (const auto value : demanded)
            mean += value / samples;

        double variance{ 0.0 };
        for (const auto value : demanded)
            variance += (value - mean) * (value - mean) / samples;

        return static_cast<float>(std::sqrt(variance));
    }
}

TEST(TestSpeedNoise, every_speed_law_demands_a_quiet_torque_current_at_a_constant_speed_on_a_4000_count_encoder)
{
    static constexpr std::array<std::tuple<const char*, foc::SpeedAlgorithm, float>, 4> rows{ {
        { "pid", foc::SpeedAlgorithm::pid, 28.0f },
        { "adrc", foc::SpeedAlgorithm::adrc, 35.0f },
        { "twodof", foc::SpeedAlgorithm::twoDof, 28.0f },
        { "lqi", foc::SpeedAlgorithm::lqi, 38.0f },
    } };

    for (const auto& [name, algorithm, limitMilliAmpere] : rows)
    {
        SCOPED_TRACE(name);
        const auto ripple = DemandedCurrentRipple(algorithm) * 1000.0f;
        std::fprintf(stderr, "[METRIC] mil speed noise %s reference=%.0f iq_demand_rms_mA=%.2f\n", name, referenceSpeed, ripple);

        EXPECT_GT(ripple, 0.0f);
        EXPECT_LT(ripple, limitMilliAmpere);
    }
}
