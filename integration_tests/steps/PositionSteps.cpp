#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the position motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}
