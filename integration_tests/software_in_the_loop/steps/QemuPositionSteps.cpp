#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the position motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

GIVEN(R"(the position motor system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId))
        << "Electrical identification rejected";
}

GIVEN(R"(the position controller is initialised with default parameters)")
{
    GTEST_SKIP() << "Position controller in-process instantiation not applicable on QEMU SIL — use CAN-based position control";
}

WHEN(R"(a position setpoint of 3.14 radians is commanded)")
{
    GTEST_SKIP() << "In-process position setpoint not applicable on QEMU SIL — use 'the CAN SetPositionSetpoint command is received'";
}

THEN(R"(the commanded duty cycles follow the position setpoint)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU SIL";
}

WHEN(R"(the position current loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable on QEMU SIL";
}

WHEN(R"(the cascade speed loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable on QEMU SIL";
}

WHEN(R"(the position loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable on QEMU SIL";
}

THEN(R"(each configured bandwidth acts on its own loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU SIL";
}

THEN(R"(the commanded duty cycles differ from those of the detuned position loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU SIL";
}
