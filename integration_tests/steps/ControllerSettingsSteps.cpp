#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/ControllerSelection.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

WHEN(R"(the {word} loop controller is set to {word} over CAN)", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;

    ASSERT_TRUE(SelectAlgorithmOverCan(fixture, *loop, *algorithm))
        << "The target refused the " << algorithmName << " " << loopName << " loop controller";
}

THEN(R"(the {word} loop controller read over CAN shall be {word})", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;

    const auto response = ReadAlgorithms(fixture);
    ASSERT_TRUE(response.has_value()) << "The target sent no controllers response";

    EXPECT_EQ((*response)[1 + static_cast<std::size_t>(*loop)], *algorithm)
        << "The " << loopName << " loop reports controller " << static_cast<int>((*response)[1 + static_cast<std::size_t>(*loop)]);
}

WHEN(R"(the {word} is set to {float} over CAN)", (std::string name, float value))
{
    auto& fixture = context.Get<Fixture>();
    const auto parameter = ParameterNamed(name);
    ASSERT_TRUE(parameter.has_value()) << "Unknown parameter: " << name;
    ASSERT_TRUE(WriteParameter(fixture, *parameter, value)) << "The target refused " << name << " = " << value;
}

WHEN(R"(the {word} is set to automatic over CAN)", (std::string name))
{
    auto& fixture = context.Get<Fixture>();
    const auto parameter = ParameterNamed(name);
    ASSERT_TRUE(parameter.has_value()) << "Unknown parameter: " << name;
    ASSERT_TRUE(WriteParameter(fixture, *parameter, 0.0f)) << "The target refused an automatic " << name;
}

WHEN(R"(the target restarts)")
{
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Only a simulated target can be restarted by a step";
    TargetInteractor::Instance().RestartTarget();

    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready after the restart";
}

THEN(R"(the {word} read over CAN shall be {float})", (std::string name, float expected))
{
    auto& fixture = context.Get<Fixture>();
    const auto parameter = ParameterNamed(name);
    ASSERT_TRUE(parameter.has_value()) << "Unknown parameter: " << name;

    const auto held = ReadParameter(fixture, *parameter);
    ASSERT_TRUE(held.has_value()) << "The target did not report " << name;
    EXPECT_FALSE(held->automatic) << name << " is reported as automatic";
    EXPECT_NEAR(held->value, expected, std::abs(expected) * 1.0e-6f) << name;
}

THEN(R"(the {word} read over CAN shall be automatic)", (std::string name))
{
    auto& fixture = context.Get<Fixture>();
    const auto parameter = ParameterNamed(name);
    ASSERT_TRUE(parameter.has_value()) << "Unknown parameter: " << name;

    const auto held = ReadParameter(fixture, *parameter);
    ASSERT_TRUE(held.has_value()) << "The target did not report " << name;
    EXPECT_TRUE(held->automatic) << name << " is reported as set to " << held->value;
    EXPECT_GT(held->value, 0.0f) << "The automatic " << name << " reports no derived value";
}

THEN(R"(setting the {word} to {float} over CAN shall be refused as a design that cannot be made)", (std::string name, float value))
{
    auto& fixture = context.Get<Fixture>();
    const auto parameter = ParameterNamed(name);
    ASSERT_TRUE(parameter.has_value()) << "Unknown parameter: " << name;

    const auto mark = fixture.MarkCapture();
    EXPECT_FALSE(WriteParameter(fixture, *parameter, value)) << "The target accepted " << name << " = " << value;

    const auto error = CapturedResponse(fixture, services::canCategoryErrorResponseMessageTypeId, mark);
    ASSERT_TRUE(error.has_value() && error->size() == 2) << "The target sent no category error";
    EXPECT_EQ((*error)[0], can::focSetParameterId);
    EXPECT_EQ((*error)[1], static_cast<uint8_t>(can::FocMotorCategoryError::invalidParameters));
}
