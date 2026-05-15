#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "integration_tests/hardware_in_the_loop/support/GdbFlasher.hpp"
#include <iostream>

HOOK_BEFORE_ALL(.name = "Initialize HIL")
{
    const auto& config = hil::GetBridgeConfig();

    hil::GdbFlasher{ config }.Flash();

    (void)hil::BridgeSession::Instance();
    std::cout << "[HIL] Bridge connected\n";
}

HOOK_BEFORE_SCENARIO()
{
}

HOOK_AFTER_SCENARIO()
{
}
