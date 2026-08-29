#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the CAN category server is connected to the state machine)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready: no heartbeat received";
}

STEP(R"(the CAN Start command is received)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focStartId))
        << "CAN Start command not acknowledged as success";
}

WHEN(R"(the CAN Stop command is received)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    fixture.SendCanCommand(can::focMotorCategoryId, can::focStopId);
}

WHEN(R"(the CAN ClearFault command is received)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    fixture.SendCanCommand(can::focMotorCategoryId, can::focClearFaultId);
}

WHEN(R"(the CAN EmergencyStop command is received)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    fixture.SendCanCommand(can::focMotorCategoryId, can::focEmergencyStopId);
}

GIVEN(R"(the CAN category server is connected to the control mode coordinator)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready: no heartbeat received";
}

GIVEN(R"(the multi-mode system is initialised with valid calibration data)")
{
    GTEST_SKIP() << "Multi-mode calibration flow not yet implemented for QEMU SIL";
}

GIVEN(R"(the active control mode is {word})", (std::string))
{
    GTEST_SKIP() << "Control mode selection via CAN not yet implemented for QEMU SIL";
}

WHEN(R"(the CAN SelectControlMode command is received with mode {word})", (std::string))
{
    GTEST_SKIP() << "SelectControlMode via CAN not yet implemented for QEMU SIL";
}

WHEN(R"(the CAN SetTorqueSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetTorqueSetpoint via CAN not yet implemented for QEMU SIL";
}

WHEN(R"(the CAN SetSpeedSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetSpeedSetpoint via CAN not yet implemented for QEMU SIL";
}

WHEN(R"(the CAN SetPositionSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "SetPositionSetpoint via CAN not yet implemented for QEMU SIL";
}

WHEN(R"(the system is restarted)")
{
    GTEST_SKIP() << "System restart not applicable on QEMU SIL";
}

THEN(R"(the active control mode shall be {word})", (std::string))
{
    GTEST_SKIP() << "Control mode not observable via CAN telemetry on QEMU SIL";
}

THEN(R"(a SelectControlModeResponse shall be emitted)")
{
    GTEST_SKIP() << "SelectControlModeResponse check not yet implemented for QEMU SIL";
}

THEN(R"(a SelectControlModeResponse shall be emitted with reason {word})", (std::string))
{
    GTEST_SKIP() << "SelectControlModeResponse check not yet implemented for QEMU SIL";
}

THEN(R"(a CommandRejected frame shall be emitted with reason controlModeMismatch)")
{
    GTEST_SKIP() << "CommandRejected frame check not yet implemented for QEMU SIL";
}

THEN(R"(no CommandRejected frame shall be emitted)")
{
    GTEST_SKIP() << "CommandRejected frame absence check not yet implemented for QEMU SIL";
}

THEN(R"(no NVM write shall occur)")
{
    GTEST_SKIP() << "NVM write count not observable on QEMU SIL";
}

WHEN(R"(a clear-calibration command is issued with deferred NVM completion)")
{
    GTEST_SKIP() << "Deferred NVM not applicable on QEMU SIL";
}

WHEN(R"(the deferred NVM invalidation completes successfully)")
{
    GTEST_SKIP() << "Deferred NVM not applicable on QEMU SIL";
}
