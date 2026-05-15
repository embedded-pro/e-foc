#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "integration_tests/hardware_in_the_loop/support/GdbFlasher.hpp"
#include <chrono>
#include <exception>
#include <iostream>

HOOK_BEFORE_ALL(.name = "Initialize HIL")
{
    try
    {
        const auto& config = hil::GetBridgeConfig();

        hil::GdbFlasher{ config }.Flash();

        auto& session = hil::BridgeSession::Instance();

        // Drain the boot banner / initial CLI prompt so the first scenario's
        // SendCommand only sees the response to its own command.
        (void)session.DrainSerial(std::chrono::milliseconds{ 500 });

        std::cout << "[HIL] Bridge connected\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HIL] BEFORE_ALL failed: " << e.what() << '\n';
        throw;
    }
}

HOOK_BEFORE_SCENARIO()
{
}

HOOK_AFTER_SCENARIO()
{
}
