#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the position motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Position motor internal state not observable via CAN protocol — host-only scenario";
}

GIVEN(R"(the position motor system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Position motor internal state not observable via CAN protocol — host-only scenario";
}

GIVEN(R"(the position controller is initialised with default parameters)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Position controller in-process instantiation not applicable on QEMU — host-only scenario";
}

WHEN(R"(a position setpoint of 3.14 radians is commanded)")
{
    GTEST_SKIP() << "Position controller in-process setpoint not applicable on QEMU — host-only scenario";
}

THEN(R"(the commanded duty cycles follow the position setpoint)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU — host-only scenario";
}

WHEN(R"(the position current loop bandwidth is configured)")
{
    GTEST_SKIP() << "Position controller in-process tuning not applicable on QEMU — host-only scenario";
}

WHEN(R"(the cascade speed loop bandwidth is configured)")
{
    GTEST_SKIP() << "Position controller in-process tuning not applicable on QEMU — host-only scenario";
}

WHEN(R"(the position loop bandwidth is configured)")
{
    GTEST_SKIP() << "Position controller in-process tuning not applicable on QEMU — host-only scenario";
}

THEN(R"(each configured bandwidth acts on its own loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU — host-only scenario";
}

THEN(R"(the commanded duty cycles differ from those of the detuned position loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU — host-only scenario";
}
