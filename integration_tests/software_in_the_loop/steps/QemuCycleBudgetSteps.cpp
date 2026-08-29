#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the QEMU SIL target is running)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available (set QEMU_SIL_ELF or build qemu-foc-sensored preset)";
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
