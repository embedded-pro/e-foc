#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/QemuSilFixture.hpp"

HOOK_BEFORE_SCENARIO()
{
    context.Emplace<sil::QemuSilFixture>();
}

HOOK_AFTER_SCENARIO()
{
}
