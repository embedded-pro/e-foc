#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

WHEN(R"(a clear-calibration command is issued with deferred NVM completion)")
{
    context.Get<StateMachineAccessor>().deferClearCalibration();
}

WHEN(R"(the deferred NVM invalidation completes successfully)")
{
    context.Get<StateMachineAccessor>().completeInvalidate(services::NvmStatus::Ok);
}
