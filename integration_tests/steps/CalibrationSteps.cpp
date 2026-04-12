#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/FocIntegrationFixture.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

GIVEN(R"(calibration service expectations are configured for success)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.SetupCalibrationExpectations();
}

GIVEN(R"(the pole-pairs estimation is configured to fail)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.SetupCalibrationExpectations();
}

WHEN(R"(the pole-pairs estimation completes with {int} pole pairs)", (int polePairs))
{
    context.Get<FocIntegrationFixture>().CompletePolePairsEstimation(
        static_cast<std::size_t>(polePairs));
}

WHEN(R"(the resistance-inductance estimation completes with resistance {int} milliohm and inductance {int} microhenry)",
    (int resistanceMilliohm, int inductanceMicrohenry))
{
    context.Get<FocIntegrationFixture>().CompleteRLEstimation(
        foc::Ohm{ static_cast<float>(resistanceMilliohm) / 1000.0f },
        foc::MilliHenry{ static_cast<float>(inductanceMicrohenry) / 1000.0f });
}

WHEN(R"(the alignment estimation completes with offset {int} radians)", (int offsetRaw))
{
    context.Get<FocIntegrationFixture>().CompleteAlignment(
        foc::Radians{ static_cast<float>(offsetRaw) });
}

WHEN(R"(the pole-pairs estimation reports failure)")
{
    auto& fixture = context.Get<FocIntegrationFixture>();
    fixture.capturedPolePairsCallback(std::nullopt);
    fixture.ExecuteAllActions();
}
