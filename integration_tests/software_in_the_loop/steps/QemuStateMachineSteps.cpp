#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    // QEMU target always boots without calibration data (ArrayEeprom is empty).
}

GIVEN(R"(the system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId))
        << "Electrical identification rejected — check state machine is in Idle";
}

STEP(R"(the enable command is issued)")
{
    GTEST_SKIP() << "Direct internal enable not available on QEMU SIL — use 'the CAN Start command is received'";
}

STEP(R"(a hardware fault is raised by the platform)")
{
    GTEST_SKIP() << "Platform fault injection not available on QEMU SIL";
}

WHEN(R"(the calibrate command is issued)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId);
}

WHEN(R"(the disable command is issued)")
{
    GTEST_SKIP() << "Direct internal disable not available on QEMU SIL — use 'the CAN Stop command is received'";
}

WHEN(R"(the clear fault command is issued)")
{
    GTEST_SKIP() << "Direct internal clear-fault not available on QEMU SIL — use 'the CAN ClearFault command is received'";
}

THEN(R"(the state machine shall be in the Idle state)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    // Both Idle and Ready map to FocMotorState::idle over CAN.
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::idle))
        << "Motor state is not idle/ready over CAN telemetry";
}

THEN(R"(the state machine shall be in the Calibrating state)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::calibrating))
        << "Motor state is not calibrating over CAN telemetry";
}

THEN(R"(the state machine shall be in the Ready state)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    // Ready and Idle both map to FocMotorState::idle over CAN — verify not fault/running.
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::idle))
        << "Motor state is not idle/ready over CAN telemetry";
}

THEN(R"(the state machine shall be in the Enabled state)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::running))
        << "Motor state is not running/enabled over CAN telemetry";
}

THEN(R"(the state machine shall be in the Fault state)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForMotorState(can::FocMotorState::fault))
        << "Motor state is not fault over CAN telemetry";
}
