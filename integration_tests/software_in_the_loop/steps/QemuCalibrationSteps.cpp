#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"
#include <gtest/gtest.h>

using namespace sil;

GIVEN(R"(calibration service expectations are configured)")
{
    auto& fixture = context.Get<QemuSilFixture>();
    if (!fixture.available)
        GTEST_SKIP() << "QEMU SIL not available";
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready";
}

WHEN(R"(the pole-pairs estimation completes with {int} pole pairs)", (int))
{
    GTEST_SKIP() << "Pole-pairs mock callback not applicable on QEMU SIL — calibration runs on real motor model";
}

WHEN(R"(the resistance-inductance estimation completes with resistance {int} milliohm and inductance {int} microhenry)",
    (int, int))
{
    GTEST_SKIP() << "R/L mock callback not applicable on QEMU SIL — calibration runs on real motor model";
}

WHEN(R"(the alignment estimation completes with offset {int} radians)", (int))
{
    GTEST_SKIP() << "Alignment mock callback not applicable on QEMU SIL — calibration runs on real motor model";
}

WHEN(R"(the pole-pairs estimation reports failure)")
{
    GTEST_SKIP() << "Failure injection not applicable on QEMU SIL";
}
