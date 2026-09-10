#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/interactor/interfaces/TargetInteractor.hpp"

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
}

HOOK_AFTER_SCENARIO()
{
    integration::TargetInteractor::Instance().AfterScenario();
}
