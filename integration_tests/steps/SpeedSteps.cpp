#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the speed motor system is initialised with no valid calibration data)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

WHEN(R"(the mechanical identification completes successfully)")
{
    GTEST_SKIP() << "Mechanical identification mock callback not applicable via transport";
}
