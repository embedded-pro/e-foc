#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(calibration service expectations are configured)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    GTEST_SKIP() << "Mock calibration service setup not applicable on QEMU — host-only scenario";
}

WHEN(R"(the pole-pairs estimation completes with {int} pole pairs)", (int))
{
    GTEST_SKIP() << "Mock calibration callback not applicable on QEMU — host-only scenario";
}

WHEN(R"(the resistance-inductance estimation completes with resistance {int} milliohm and inductance {int} microhenry)",
    (int, int))
{
    GTEST_SKIP() << "Mock calibration callback not applicable on QEMU — host-only scenario";
}

WHEN(R"(the alignment estimation completes with offset {int} radians)", (int))
{
    GTEST_SKIP() << "Mock calibration callback not applicable on QEMU — host-only scenario";
}

WHEN(R"(the pole-pairs estimation reports failure)")
{
    GTEST_SKIP() << "Mock calibration callback not applicable on QEMU — host-only scenario";
}
