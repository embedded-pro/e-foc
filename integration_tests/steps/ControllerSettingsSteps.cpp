#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

namespace
{
    constexpr auto kSettingTimeout = std::chrono::seconds{ 10 };
    constexpr auto kResponseTimeout = std::chrono::seconds{ 5 };

    hal::Can::Id ResponseId(const Fixture& fixture, uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, can::focMotorCategoryId, messageType, fixture.ServerNodeId()));
    }

    std::optional<can::FocControlLoop> LoopNamed(const std::string& name)
    {
        if (name == "current")
            return can::FocControlLoop::current;
        if (name == "speed")
            return can::FocControlLoop::speed;
        if (name == "position")
            return can::FocControlLoop::position;
        return std::nullopt;
    }

    std::optional<uint8_t> AlgorithmNamed(can::FocControlLoop loop, const std::string& name)
    {
        static constexpr std::array<const char*, 4> current{ "pid", "decoupled", "deadbeat", "sliding" };
        static constexpr std::array<const char*, 4> speed{ "pid", "lqi", "adrc", "twodof" };
        static constexpr std::array<const char*, 5> position{ "pid", "cascadep", "lqr", "lqi", "twodof" };

        const auto find = [&name](const auto& names) -> std::optional<uint8_t>
        {
            for (uint8_t index = 0; index != names.size(); ++index)
                if (name == names[index])
                    return index;
            return std::nullopt;
        };

        if (loop == can::FocControlLoop::current)
            return find(current);
        if (loop == can::FocControlLoop::speed)
            return find(speed);
        return find(position);
    }

    std::optional<can::FocParameter> ParameterNamed(const std::string& name)
    {
        for (const auto& descriptor : can::focParameters)
            if (name == descriptor.name)
                return descriptor.id;
        return std::nullopt;
    }

    std::optional<hal::Can::Message> CapturedResponse(Fixture& fixture, uint8_t messageType, Fixture::CaptureMark mark)
    {
        const auto deadline = std::chrono::steady_clock::now() + kResponseTimeout;
        while (true)
        {
            if (auto response = fixture.FindCapturedCanFrame(ResponseId(fixture, messageType), mark))
                return response;
            if (std::chrono::steady_clock::now() >= deadline)
                return std::nullopt;
            fixture.DrainLines(std::chrono::milliseconds{ 50 });
        }
    }

    bool WriteParameter(Fixture& fixture, can::FocParameter parameter, float value)
    {
        const auto bits = std::bit_cast<uint32_t>(value);
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(parameter));
        for (int shift = 24; shift >= 0; shift -= 8)
            payload.push_back(static_cast<uint8_t>(bits >> shift));
        return fixture.SendCanCommand(can::focMotorCategoryId, can::focSetParameterId, payload, kSettingTimeout);
    }

    std::optional<can::FocParameterValue> ReadParameter(Fixture& fixture, can::FocParameter parameter)
    {
        const auto mark = fixture.MarkCapture();
        hal::Can::Message payload;
        payload.push_back(static_cast<uint8_t>(parameter));
        if (!fixture.SendCanCommand(can::focMotorCategoryId, can::focQueryParameterId, payload, kResponseTimeout))
            return std::nullopt;

        const auto response = CapturedResponse(fixture, can::focParameterResponseId, mark);
        if (!response.has_value() || response->size() != 6 || (*response)[0] != static_cast<uint8_t>(parameter))
            return std::nullopt;

        uint32_t bits{ 0 };
        for (std::size_t index = 2; index != 6; ++index)
            bits = (bits << 8) | (*response)[index];
        return can::FocParameterValue{ std::bit_cast<float>(bits), ((*response)[1] & can::focParameterAutomaticFlag) != 0 };
    }
}

WHEN(R"(the {word} loop controller is set to {word} over CAN)", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;

    hal::Can::Message payload;
    payload.push_back(static_cast<uint8_t>(*loop));
    payload.push_back(*algorithm);
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focSelectAlgorithmId, payload, kSettingTimeout))
        << "The target refused the " << algorithmName << " " << loopName << " loop controller";
}

THEN(R"(the {word} loop controller read over CAN shall be {word})", (std::string loopName, std::string algorithmName))
{
    auto& fixture = context.Get<Fixture>();
    const auto loop = LoopNamed(loopName);
    ASSERT_TRUE(loop.has_value()) << "Unknown control loop: " << loopName;
    const auto algorithm = AlgorithmNamed(*loop, algorithmName);
    ASSERT_TRUE(algorithm.has_value()) << "Unknown " << loopName << " loop algorithm: " << algorithmName;

    const auto mark = fixture.MarkCapture();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focQueryAlgorithmsId, {}, kResponseTimeout))
        << "The target did not answer the controller query";
    const auto response = CapturedResponse(fixture, can::focAlgorithmsResponseId, mark);
    ASSERT_TRUE(response.has_value() && response->size() == 4) << "The target sent no controllers response";

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
