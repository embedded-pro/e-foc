#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(the QEMU SIL target is running)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available (set QEMU_SIL_ELF or build qemu-cortex-m4 preset)";
}

WHEN(R"(the FOC hot paths are benchmarked on the emulated target)")
{
    context.Get<QemuSilFixture>().RunPerformanceBenchmark();
}

THEN(R"(the torque Calculate\(\) cycle count is recorded)")
{
    const auto cycles = context.Get<QemuSilFixture>().CycleCount("torque_calculate");
    std::printf("[[CYCLES]] torque_calculate=%lu\n", static_cast<unsigned long>(cycles));
    EXPECT_GT(cycles, 0u) << "No DWT torque_calculate measurement received from QEMU";
}

THEN(R"(the speed Calculate\(\) cycle count is recorded)")
{
    const auto cycles = context.Get<QemuSilFixture>().CycleCount("speed_calculate");
    std::printf("[[CYCLES]] speed_calculate=%lu\n", static_cast<unsigned long>(cycles));
    EXPECT_GT(cycles, 0u) << "No DWT speed_calculate measurement received from QEMU";
}

THEN(R"(the position Calculate\(\) cycle count is recorded)")
{
    const auto cycles = context.Get<QemuSilFixture>().CycleCount("position_calculate");
    std::printf("[[CYCLES]] position_calculate=%lu\n", static_cast<unsigned long>(cycles));
    EXPECT_GT(cycles, 0u) << "No DWT position_calculate measurement received from QEMU";
}
