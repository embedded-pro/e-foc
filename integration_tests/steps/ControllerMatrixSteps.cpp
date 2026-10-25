#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/ControllerSelection.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

namespace
{
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
    const auto selected = AlgorithmNamed(can::FocControlLoop::current, algorithm);
    ASSERT_TRUE(selected.has_value()) << "Unknown current loop algorithm: " << algorithm;
    setup.nvm.includeConfig = true;
    setup.nvm.config.currentAlgorithm = *selected;
}

GIVEN(R"(the speed loop runs the {word} algorithm)", (std::string algorithm))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = AlgorithmNamed(can::FocControlLoop::speed, algorithm);
    ASSERT_TRUE(selected.has_value()) << "Unknown speed loop algorithm: " << algorithm;
    setup.nvm.includeConfig = true;
    setup.nvm.config.speedAlgorithm = *selected;
}

GIVEN(R"(the position loop runs the {word} algorithm)", (std::string algorithm))
{
    auto& setup = context.Get<ScenarioSetup>();
    const auto selected = AlgorithmNamed(can::FocControlLoop::position, algorithm);
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

WHEN(R"(electrical identification is attempted)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId, {}, std::chrono::seconds{ 60 });
}

WHEN(R"(the rotor alignment is attempted)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focAlignId, {}, std::chrono::seconds{ 30 });
}

THEN(R"(the {word} loop shall be running the {word} algorithm)", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;

    const auto response = ReadAlgorithms(fixture);
    ASSERT_TRUE(response.has_value()) << "The target sent no controllers response";

    const auto running = (*response)[1 + static_cast<std::size_t>(*loop)];
    EXPECT_EQ(running, *algorithm)
        << "The " << loopName << " loop is running algorithm " << static_cast<int>(running) << " rather than the selected " << algorithmName;
}
