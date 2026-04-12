#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

GIVEN(R"(the CAN category server is connected to the state machine)")
{
    context.Get<StateMachineAccessor>().setupCanIntegration();
}

STEP(R"(the CAN Start command is received)")
{
    context.Get<StateMachineAccessor>().injectCanStart();
}

WHEN(R"(the CAN Stop command is received)")
{
    context.Get<StateMachineAccessor>().injectCanStop();
}

WHEN(R"(the CAN ClearFault command is received)")
{
    context.Get<StateMachineAccessor>().injectCanClearFault();
}
