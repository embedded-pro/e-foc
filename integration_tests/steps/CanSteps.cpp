#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

using namespace integration;

namespace
{
    constexpr uint32_t expectedCanId{ 256 };
    constexpr std::array<uint8_t, 3> expectedCanPayload{ 0x01, 0x02, 0x03 };
}

GIVEN(R"(the CAN category server is connected to the state machine)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready: no heartbeat received";
}

STEP(R"(the CAN Start command is received)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId))
        << "CAN Start command not acknowledged as success";
}

WHEN(R"(the CAN Stop command is received)")
{
    context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focStopId);
}

WHEN(R"(the CAN ClearFault command is received)")
{
    context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focClearFaultId);
}

WHEN(R"(the CAN EmergencyStop command is received)")
{
    context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focEmergencyStopId);
}

GIVEN(R"(the CAN category server is connected to the control mode coordinator)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready: no heartbeat received";
}

GIVEN(R"(the multi-mode system is initialised with valid calibration data)")
{
    GTEST_SKIP() << "Multi-mode calibration flow not yet implemented for SIL";
}

GIVEN(R"(the active control mode is {word})", (std::string))
{
    GTEST_SKIP() << "Control mode selection via CAN not yet implemented for SIL";
}

WHEN(R"(the CAN SelectControlMode command is received with mode {word})", (std::string))
{
    GTEST_SKIP() << "SelectControlMode via CAN not yet implemented for SIL";
}

WHEN(R"(the CAN SetTorqueSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetTorqueSetpoint via CAN not yet implemented for SIL";
}

WHEN(R"(the CAN SetSpeedSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetSpeedSetpoint via CAN not yet implemented for SIL";
}

WHEN(R"(the CAN SetPositionSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetPositionSetpoint via CAN not yet implemented for SIL";
}

WHEN(R"(the system is restarted)")
{
    GTEST_SKIP() << "System restart not applicable on SIL";
}

THEN(R"(the active control mode shall be {word})", (std::string))
{
    GTEST_SKIP() << "Control mode not observable via CAN telemetry on SIL";
}

THEN(R"(a SelectControlModeResponse shall be emitted)")
{
    GTEST_SKIP() << "SelectControlModeResponse check not yet implemented for SIL";
}

THEN(R"(a SelectControlModeResponse shall be emitted with reason {word})", (std::string))
{
    GTEST_SKIP() << "SelectControlModeResponse check not yet implemented for SIL";
}

THEN(R"(a CommandRejected frame shall be emitted with reason controlModeMismatch)")
{
    GTEST_SKIP() << "CommandRejected frame check not yet implemented for SIL";
}

THEN(R"(no CommandRejected frame shall be emitted)")
{
    GTEST_SKIP() << "CommandRejected frame absence check not yet implemented for SIL";
}

THEN(R"(no NVM write shall occur)")
{
    GTEST_SKIP() << "NVM write count not observable via transport";
}

WHEN(R"(a clear-calibration command is issued with deferred NVM completion)")
{
    GTEST_SKIP() << "Deferred NVM not applicable via transport";
}

WHEN(R"(the deferred NVM invalidation completes successfully)")
{
    GTEST_SKIP() << "Deferred NVM not applicable via transport";
}

WHEN(R"(a CAN command is transmitted to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();

    fixture.StartCanCapture();

    ASSERT_TRUE(fixture.SendCommand("can_start 500000", hil::timeouts::command))
        << "can_start command did not receive a response";

    ASSERT_TRUE(fixture.SendCommand("can_send 256 1 2 3", hil::timeouts::command))
        << "can_send command did not receive a response within timeout";

    fixture.MarkCanReference();

    fixture.FlushPartialLines(hil::timeouts::command);

    const bool acknowledged = std::any_of(fixture.allLines.begin(), fixture.allLines.end(),
        [](const std::string& l)
        {
            return l.find("CAN frame sent") != std::string::npos;
        });
    ASSERT_TRUE(acknowledged) << "CLI did not acknowledge can_send";
}

THEN(R"(a CAN response frame shall be received within 10 milliseconds)")
{
    auto& fixture = context.Get<Fixture>();

    hal::Can::Message payload;
    std::chrono::milliseconds elapsed{ 0 };
    const auto expectedId = hal::Can::Id::Create11BitId(expectedCanId);

    ASSERT_TRUE(fixture.WaitForCanFrame(expectedId, payload,
        std::chrono::milliseconds{ 10 }, elapsed))
        << "No CAN frame with id 0x" << std::hex << expectedCanId
        << " observed on the bus within 10 ms";

    ASSERT_EQ(payload.size(), expectedCanPayload.size())
        << "CAN frame payload length mismatch";
    for (std::size_t i = 0; i < expectedCanPayload.size(); ++i)
        EXPECT_EQ(payload[i], expectedCanPayload[i])
            << "CAN payload byte " << i << " mismatch";

    EXPECT_LE(elapsed.count(), 10)
        << "CAN response latency " << elapsed.count()
        << " ms exceeds the 10 ms budget";
}
