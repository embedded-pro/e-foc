#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/FocIntegrationFixture.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

GIVEN(R"(the CAN category server is connected to the state machine)")
{
    context.Get<FocIntegrationFixture>().SetupCanIntegration();
}

STEP(R"(the CAN Start command is received)")
{
    context.Get<FocIntegrationFixture>().InjectCanStart();
}

WHEN(R"(the CAN Stop command is received)")
{
    context.Get<FocIntegrationFixture>().InjectCanStop();
}

WHEN(R"(the CAN ClearFault command is received)")
{
    context.Get<FocIntegrationFixture>().InjectCanClearFault();
}
