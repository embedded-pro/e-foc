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

namespace
{
    std::string LoopTraceLabel(const std::string& loop)
    {
        if (loop == "current")
            return "[SM] Current loop algorithm: ";
        if (loop == "speed")
            return "[SM] Speed loop algorithm: ";
        if (loop == "position")
            return "[SM] Position loop algorithm: ";
        return {};
    }
}

THEN(R"(the {word} loop shall be running the {word} algorithm)", (std::string loop, std::string algorithm))
{
    auto& fixture = context.Get<Fixture>();
    const auto label = LoopTraceLabel(loop);
    ASSERT_FALSE(label.empty()) << "Unknown control loop: " << loop;

    // The firmware reports the algorithm that actually took effect, which differs from the one
    // that was asked for when its design does not converge for this motor. It traces that on
    // entry to Ready, and its tracer only drains while the event loop has work, so poll telemetry
    // rather than waiting passively for the line to arrive.
    std::string reported;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
    while (reported.empty() && std::chrono::steady_clock::now() < deadline)
    {
        fixture.WaitForMotorState(can::FocMotorState::idle, std::chrono::milliseconds{ 200 });
        fixture.DrainLines(std::chrono::milliseconds{ 100 });

        for (const auto& line : fixture.allLines)
        {
            const auto position = line.find(label);
            if (position != std::string::npos)
                reported = line.substr(position + label.size());
        }
    }

    ASSERT_FALSE(reported.empty()) << "Firmware never reported the active " << loop << " loop algorithm";
    while (!reported.empty() && (reported.back() == '\r' || reported.back() == ' '))
        reported.pop_back();

    EXPECT_EQ(reported, algorithm)
        << "The " << loop << " loop is running " << reported << " rather than the selected " << algorithm;
}
