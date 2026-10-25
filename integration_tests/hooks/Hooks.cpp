#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"
#include <utility>

HOOK_BEFORE_ALL(.name = "Setup target")
{
    integration::TargetInteractor::Instance().Setup();
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

HOOK_AFTER_SCENARIO()
{
    auto& interactor = integration::TargetInteractor::Instance();
    if (hasError)
        if (auto log = interactor.DebugLog(); !log.empty())
            Attach(std::move(log), cucumber_cpp::library::engine::AttachOptions{ "text/plain", "target-debug-output.log" });

    interactor.AfterScenario();
}
