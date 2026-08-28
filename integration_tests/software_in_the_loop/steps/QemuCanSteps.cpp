#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the CAN category server is connected to the state machine)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "In-process CAN server setup not applicable on QEMU — host-only scenario";
}

STEP(R"(the CAN Start command is received)")
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN Stop command is received)")
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN ClearFault command is received)")
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN EmergencyStop command is received)")
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

GIVEN(R"(the CAN category server is connected to the control mode coordinator)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "In-process control mode coordinator setup not applicable on QEMU — host-only scenario";
}

GIVEN(R"(the multi-mode system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Multi-mode system in-process instantiation not applicable on QEMU — host-only scenario";
}

GIVEN(R"(the active control mode is {word})", (std::string))
{
    GTEST_SKIP() << "Control mode selection not observable on QEMU — host-only scenario";
}

WHEN(R"(the CAN SelectControlMode command is received with mode {word})", (std::string))
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN SetTorqueSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN SetSpeedSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the CAN SetPositionSetpoint command is received with value {int})", (int32_t))
{
    GTEST_SKIP() << "In-process CAN injection not applicable on QEMU — host-only scenario";
}

WHEN(R"(the system is restarted)")
{
    GTEST_SKIP() << "System restart not applicable on QEMU — host-only scenario";
}

THEN(R"(the active control mode shall be {word})", (std::string))
{
    GTEST_SKIP() << "Control mode not observable on QEMU — host-only scenario";
}

THEN(R"(a SelectControlModeResponse shall be emitted)")
{
    GTEST_SKIP() << "In-process response check not applicable on QEMU — host-only scenario";
}

THEN(R"(a SelectControlModeResponse shall be emitted with reason {word})", (std::string))
{
    GTEST_SKIP() << "In-process response check not applicable on QEMU — host-only scenario";
}

THEN(R"(a CommandRejected frame shall be emitted with reason controlModeMismatch)")
{
    GTEST_SKIP() << "In-process response check not applicable on QEMU — host-only scenario";
}

THEN(R"(no CommandRejected frame shall be emitted)")
{
    GTEST_SKIP() << "In-process response check not applicable on QEMU — host-only scenario";
}

THEN(R"(no NVM write shall occur)")
{
    GTEST_SKIP() << "NVM write count not observable on QEMU — host-only scenario";
}

WHEN(R"(a clear-calibration command is issued with deferred NVM completion)")
{
    GTEST_SKIP() << "Deferred NVM completion not applicable on QEMU — host-only scenario";
}

WHEN(R"(the deferred NVM invalidation completes successfully)")
{
    GTEST_SKIP() << "Deferred NVM completion not applicable on QEMU — host-only scenario";
}
