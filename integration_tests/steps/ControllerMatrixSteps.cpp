#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

namespace
{
    // The tokens match the CLI algorithm names documented in the controller-selection design.
    std::optional<uint8_t> CurrentAlgorithmFromName(const std::string& name)
    {
        if (name == "pid")
            return 0;
        if (name == "decoupled")
            return 1;
        if (name == "deadbeat")
            return 2;
        if (name == "sliding")
            return 3;
        return std::nullopt;
    }

    std::optional<uint8_t> SpeedAlgorithmFromName(const std::string& name)
    {
        if (name == "pid")
            return 0;
        if (name == "lqi")
            return 1;
        if (name == "adrc")
            return 2;
        if (name == "twodof")
            return 3;
        return std::nullopt;
    }

    std::optional<uint8_t> PositionAlgorithmFromName(const std::string& name)
    {
        if (name == "pid")
            return 0;
        if (name == "cascadep")
            return 1;
        if (name == "lqr")
            return 2;
        if (name == "lqi")
            return 3;
        if (name == "twodof")
            return 4;
        return std::nullopt;
    }

    std::optional<uint8_t> ControlModeFromName(const std::string& name)
    {
        if (name == "torque")
            return static_cast<uint8_t>(can::FocMotorMode::torque);
        if (name == "speed")
            return static_cast<uint8_t>(can::FocMotorMode::speed);
        if (name == "position")
            return static_cast<uint8_t>(can::FocMotorMode::position);
        return std::nullopt;
    }
}

GIVEN(R"(the current loop runs the {word} algorithm)", (std::string algorithm))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = CurrentAlgorithmFromName(algorithm);
    ASSERT_TRUE(selected.has_value()) << "Unknown current loop algorithm: " << algorithm;
    setup.nvm.includeConfig = true;
    setup.nvm.config.currentAlgorithm = *selected;
}

GIVEN(R"(the speed loop runs the {word} algorithm)", (std::string algorithm))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = SpeedAlgorithmFromName(algorithm);
    ASSERT_TRUE(selected.has_value()) << "Unknown speed loop algorithm: " << algorithm;
    setup.nvm.includeConfig = true;
    setup.nvm.config.speedAlgorithm = *selected;
}

GIVEN(R"(the position loop runs the {word} algorithm)", (std::string algorithm))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = PositionAlgorithmFromName(algorithm);
    ASSERT_TRUE(selected.has_value()) << "Unknown position loop algorithm: " << algorithm;
    setup.nvm.includeConfig = true;
    setup.nvm.config.positionAlgorithm = *selected;
}

GIVEN(R"(the motor boots in {word} mode)", (std::string mode))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = ControlModeFromName(mode);
    ASSERT_TRUE(selected.has_value()) << "Unknown control mode: " << mode;
    setup.nvm.includeConfig = true;
    setup.nvm.config.defaultControlMode = *selected;
}

WHEN(R"(the rotor is aligned)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focAlignId, {}, std::chrono::seconds{ 30 }))
        << "Align command rejected";
}
