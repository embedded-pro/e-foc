#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the position motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

GIVEN(R"(the position motor system is initialised with valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
    ASSERT_TRUE(fixture.SendCanCommand(can::focMotorCategoryId, can::focIdentifyElectricalId))
        << "Electrical identification rejected";
}

GIVEN(R"(the position controller is initialised with default parameters)")
{
    GTEST_SKIP() << "Position controller in-process instantiation not applicable — use CAN-based position control";
}

WHEN(R"(a position setpoint of 3.14 radians is commanded)")
{
    GTEST_SKIP() << "In-process position setpoint not applicable — use 'the CAN SetPositionSetpoint command is received'";
}

THEN(R"(the commanded duty cycles follow the position setpoint)")
{
    GTEST_SKIP() << "Duty cycle introspection not available via transport";
}

WHEN(R"(the position current loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable via transport";
}

WHEN(R"(the cascade speed loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable via transport";
}

WHEN(R"(the position loop bandwidth is configured)")
{
    GTEST_SKIP() << "In-process bandwidth tuning not applicable via transport";
}

THEN(R"(each configured bandwidth acts on its own loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available via transport";
}

THEN(R"(the commanded duty cycles differ from those of the detuned position loop)")
{
    GTEST_SKIP() << "Duty cycle introspection not available via transport";
}
