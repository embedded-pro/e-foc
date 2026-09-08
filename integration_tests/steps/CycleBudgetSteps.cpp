#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <gtest/gtest.h>

using namespace integration;

GIVEN(R"(the QEMU SIL target is running)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

WHEN(R"(the FOC hot paths are benchmarked on the emulated target)")
{
    GTEST_SKIP() << "Cycle budget benchmarks require DWT CYCCNT support — not yet implemented in SIL target";
}

THEN(R"(the torque Calculate\(\) cycle count is recorded)")
{
    GTEST_SKIP() << "Cycle budget benchmarks not yet implemented";
}

THEN(R"(the speed Calculate\(\) cycle count is recorded)")
{
    GTEST_SKIP() << "Cycle budget benchmarks not yet implemented";
}

THEN(R"(the position Calculate\(\) cycle count is recorded)")
{
    GTEST_SKIP() << "Cycle budget benchmarks not yet implemented";
}
