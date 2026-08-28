#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the speed motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Speed motor internal state not observable via CAN protocol — host-only scenario";
}

GIVEN(R"(the speed motor system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Speed motor internal state not observable via CAN protocol — host-only scenario";
}

WHEN(R"(the mechanical identification completes successfully)")
{
    GTEST_SKIP() << "Mock mechanical identification callback not applicable on QEMU — host-only scenario";
}

GIVEN(R"(the speed controller is initialised with default parameters)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Speed controller in-process instantiation not applicable on QEMU — host-only scenario";
}

WHEN(R"(a velocity setpoint of 100 radians per second is commanded)")
{
    GTEST_SKIP() << "Speed controller in-process setpoint not applicable on QEMU — host-only scenario";
}

THEN(R"(the commanded duty cycles follow the velocity setpoint)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU — host-only scenario";
}

WHEN(R"(a current loop bandwidth well below the baseline is configured)")
{
    GTEST_SKIP() << "Speed controller in-process tuning not applicable on QEMU — host-only scenario";
}

THEN(R"(the commanded duty cycles differ from those of the baseline bandwidth)")
{
    GTEST_SKIP() << "Duty cycle introspection not available on QEMU — host-only scenario";
}

WHEN(R"(the speed loop bandwidth is configured)")
{
    GTEST_SKIP() << "Speed controller in-process tuning not applicable on QEMU — host-only scenario";
}

THEN(R"(the outer loop frequency is 1000 Hz)")
{
    GTEST_SKIP() << "Speed controller internal state not observable on QEMU — host-only scenario";
}
