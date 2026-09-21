#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <unistd.h>

using namespace integration;

namespace
{
    std::optional<can::FocFaultCode> FaultCodeFromName(const std::string& name)
    {
        if (name == "overcurrent")
            return can::FocFaultCode::overCurrent;
        if (name == "overvoltage")
            return can::FocFaultCode::overVoltage;
        if (name == "undervoltage")
            return can::FocFaultCode::underVoltage;
        if (name == "overtemperature")
            return can::FocFaultCode::overTemperature;
        if (name == "sensor")
            return can::FocFaultCode::sensorFault;
        return std::nullopt;
    }
}

THEN(R"(the state machine shall report an {word} fault)", (std::string fault))
{
    auto& fixture = context.Get<Fixture>();
    const auto expected = FaultCodeFromName(fault);
    ASSERT_TRUE(expected.has_value()) << "Unknown fault: " << fault;

    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::fault, std::chrono::seconds{ 10 }))
        << "Motor never reached the fault state over CAN telemetry";
    ASSERT_TRUE(fixture.WaitForFaultCode(*expected, std::chrono::seconds{ 5 }))
        << "Motor faulted, but not with the expected " << fault << " code";
}

THEN(R"(the motor shall refuse to run)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId);
    ASSERT_FALSE(fixture.WaitForMotorState(can::FocMotorState::running, std::chrono::seconds{ 2 }))
        << "Motor reached the running state despite the fault condition";
}

WHEN(R"(the motor start command is issued)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId))
        << "Start command rejected";
}

WHEN(R"(the fault is cleared)")
{
    auto& fixture = context.Get<Fixture>();
    fixture.SendCanCommand(can::focMotorCategoryId, can::focClearFaultId);
}

THEN(R"(the rotor shall not turn)")
{
    auto& fixture = context.Get<Fixture>();

    const auto start = fixture.ReadMeasuredPosition();
    ASSERT_TRUE(start.has_value()) << "No position in the status telemetry frame";

    usleep(500000);

    const auto end = fixture.ReadMeasuredPosition();
    ASSERT_TRUE(end.has_value()) << "No position in the status telemetry frame";

    EXPECT_NEAR(*start, *end, 0.05f)
        << "The rotor moved from " << *start << " rad to " << *end << " rad";
}

THEN(R"(the rotor shall turn)")
{
    auto& fixture = context.Get<Fixture>();

    const auto start = fixture.ReadMeasuredPosition();
    ASSERT_TRUE(start.has_value()) << "No position in the status telemetry frame";

    // Telemetry position is quantised to 0.01 rad, so the reading can land exactly on the threshold.
    constexpr float kMinimumMovement = 0.05f;

    float moved = 0.0f;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
    while (std::chrono::steady_clock::now() < deadline && moved < kMinimumMovement)
    {
        usleep(200000);
        const auto now = fixture.ReadMeasuredPosition();
        if (now.has_value())
            moved = std::fabs(*now - *start);
    }

    EXPECT_GE(moved, kMinimumMovement) << "The rotor never moved away from " << *start << " rad";
}
