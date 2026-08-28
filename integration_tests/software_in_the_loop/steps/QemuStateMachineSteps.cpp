#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

GIVEN(R"(the system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

STEP(R"(the enable command is issued)")
{
    GTEST_SKIP() << "State machine internal command not available on QEMU — host-only scenario";
}

STEP(R"(a hardware fault is raised by the platform)")
{
    GTEST_SKIP() << "Platform fault injection not available on QEMU — host-only scenario";
}

WHEN(R"(the calibrate command is issued)")
{
    GTEST_SKIP() << "State machine internal command not available on QEMU — host-only scenario";
}

WHEN(R"(the disable command is issued)")
{
    GTEST_SKIP() << "State machine internal command not available on QEMU — host-only scenario";
}

WHEN(R"(the clear fault command is issued)")
{
    GTEST_SKIP() << "State machine internal command not available on QEMU — host-only scenario";
}

THEN(R"(the state machine shall be in the Idle state)")
{
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

THEN(R"(the state machine shall be in the Calibrating state)")
{
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

THEN(R"(the state machine shall be in the Ready state)")
{
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

THEN(R"(the state machine shall be in the Enabled state)")
{
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}

THEN(R"(the state machine shall be in the Fault state)")
{
    GTEST_SKIP() << "State machine internal state not observable via CAN protocol — host-only scenario";
}
