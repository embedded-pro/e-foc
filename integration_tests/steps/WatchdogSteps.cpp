#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

namespace
{
    constexpr auto kRebootTimeout = std::chrono::seconds{ 60 };

    bool ContainsLine(const Fixture& fixture, const std::string& text)
    {
        for (const auto& line : fixture.allLines)
            if (line.find(text) != std::string::npos)
                return true;

        return false;
    }
}

GIVEN(R"(the event loop of the emulated target stalls {int} ms after it boots)", (int delayMs))
{
    ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant()) << "Only the emulated target's world can stall its event loop";
    ASSERT_GT(delayMs, 0) << "A stall needs a delay after boot";
    context.Get<ScenarioSetup>().plant.eventLoopStallDelayMs = static_cast<uint32_t>(delayMs);
}

THEN(R"(the emulated target reboots and reports the reset cause Watchdog)")
{
    auto& fixture = context.Get<Fixture>();
    const auto watchdog = static_cast<long>(can::FocResetCause::watchdog);
    const auto deadline = std::chrono::steady_clock::now() + kRebootTimeout;

    std::optional<float> cause;
    while (std::chrono::steady_clock::now() < deadline)
    {
        cause = ReadParameterValue(fixture, can::FocParameter::resetCause);
        if (cause.has_value() && std::lround(*cause) == watchdog)
            break;
    }

    ASSERT_TRUE(cause.has_value()) << "The emulated target did not come back after the stall";
    EXPECT_EQ(std::lround(*cause), watchdog) << "The emulated target reports reset cause " << *cause << " after the stall";
}

WHEN(R"(the watchdog_stall command is sent to the hardware target)")
{
    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.SendCommand("watchdog_stall", hil::timeouts::command))
        << "watchdog_stall command did not respond";
    ASSERT_TRUE(ContainsLine(fixture, "[WDT] stalling event loop"))
        << "watchdog_stall did not report that the event loop is about to stall";
}
