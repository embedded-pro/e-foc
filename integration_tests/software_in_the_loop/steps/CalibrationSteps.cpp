#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

GIVEN(R"(calibration service expectations are configured)")
{
    auto& accessor = context.Get<StateMachineAccessor>();
    accessor.setupCalibrationExpectations();
    accessor.calibrationExpectationsConfigured = true;
}

WHEN(R"(the pole-pairs estimation completes with {int} pole pairs)", (int polePairs))
{
    context.Get<StateMachineAccessor>().completePolePairsEstimation(
        static_cast<std::size_t>(polePairs));
}

WHEN(R"(the resistance-inductance estimation completes with resistance {int} milliohm and inductance {int} microhenry)",
    (int resistanceMilliohm, int inductanceMicrohenry))
{
    context.Get<StateMachineAccessor>().completeRLEstimation(
        foc::Ohm{ static_cast<float>(resistanceMilliohm) / 1000.0f },
        foc::MilliHenry{ static_cast<float>(inductanceMicrohenry) / 1000.0f });
}

WHEN(R"(the alignment estimation completes with offset {int} radians)", (int offsetRaw))
{
    context.Get<StateMachineAccessor>().completeAlignment(
        foc::Radians{ static_cast<float>(offsetRaw) });
}

WHEN(R"(the pole-pairs estimation reports failure)")
{
    context.Get<StateMachineAccessor>().completePolePairsFailure();
}
