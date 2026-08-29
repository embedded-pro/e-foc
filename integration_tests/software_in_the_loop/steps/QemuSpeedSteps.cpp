#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the speed motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

GIVEN(R"(the speed motor system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId))
        << "Electrical identification rejected";
}

WHEN(R"(the mechanical identification completes successfully)")
{
    GTEST_SKIP() << "Mechanical identification mock callback not applicable on QEMU SIL";
}

GIVEN(R"(the speed controller is initialised with default parameters)")
{
    GTEST_SKIP() << "Speed controller in-process instantiation not applicable on QEMU SIL — use CAN-based speed control";
}

WHEN(R"(a velocity setpoint of 100 radians per second is commanded)")
{
    GTEST_SKIP() << "In-process speed setpoint not applicable on QEMU SIL — use 'the CAN SetSpeedSetpoint command is received'";
}

THEN(R"(the commanded duty cycles follow the velocity setpoint)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU SIL";
}

WHEN(R"(a current loop bandwidth well below the baseline is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable on QEMU SIL";
}

THEN(R"(the commanded duty cycles differ from those of the baseline bandwidth)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU SIL";
}

WHEN(R"(the speed loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable on QEMU SIL";
}

THEN(R"(the outer loop frequency is 1000 Hz)")
{
    GTEST_SKIP() << "Speed controller internal state not observable on QEMU SIL";
}
