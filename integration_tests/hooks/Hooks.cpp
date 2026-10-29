#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/TerminalFixture.hpp"
#include "integration_tests/support/interactor/hardware/BridgeConfig.hpp"
#include "integration_tests/support/interactor/hardware/Timeouts.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include "integration_tests/support/interactor/interfaces/TerminalInteractor.hpp"
#include <cstdio>
#include <exception>
#include <gtest/gtest.h>
#include <utility>

HOOK_BEFORE_ALL(.name = "Setup target")
{
    try
    {
        integration::TargetInteractor::Instance().Setup();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Setup target failed: %s\n", e.what());
        throw;
    }
}

HOOK_AFTER_ALL(.name = "Teardown target")
{
    integration::TargetInteractor::Instance().Teardown();
}

HOOK_BEFORE_SCENARIO()
{
    integration::TargetInteractor::Instance().BeforeScenario();
    context.Emplace<integration::Fixture>();
    context.Emplace<integration::ScenarioSetup>();
}

HOOK_BEFORE_SCENARIO(.expression = "@hil", .name = "Bring-up terminal")
{
    auto* terminal = integration::TerminalInteractor::Instance();
    ASSERT_NE(terminal, nullptr) << "The bring-up scenarios need a target with a terminal";

    const auto fixture = context.Emplace<integration::TerminalFixture>(*terminal);
    if (hil::GetBridgeConfig().restartPerScenario)
        return;

    (void)fixture->WaitForPrompt(hil::timeouts::promptShort);
    (void)fixture->SendCommand("stop", hil::timeouts::cleanup);
    (void)fixture->SendCommand("can_stop", hil::timeouts::cleanup);
    (void)fixture->WaitForPrompt(hil::timeouts::cleanup);
}

HOOK_AFTER_SCENARIO()
{
    auto& interactor = integration::TargetInteractor::Instance();
    if (hasError)
        if (auto log = interactor.DebugLog(); !log.empty())
            Attach(std::move(log), cucumber_cpp::library::engine::AttachOptions{ "text/plain", "target-debug-output.log" });

    interactor.AfterScenario();
}
