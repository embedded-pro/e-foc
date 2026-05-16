#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include "integration_tests/hardware_in_the_loop/support/BridgeSession.hpp"
#include "integration_tests/hardware_in_the_loop/support/GdbFlasher.hpp"
#include "integration_tests/hardware_in_the_loop/support/SerialLogger.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <exception>
#include <iostream>

HOOK_BEFORE_ALL(.name = "Initialize HIL")
{
    try
    {
        const auto& config = hil::GetBridgeConfig();

        hil::SerialLogger::Instance().OpenSession();
        hil::SerialLogger::Instance().Note("HOOK", "BEFORE_ALL: flashing firmware");

        hil::GdbFlasher{ config }.Flash();

        auto& session = hil::BridgeSession::Instance();
        (void)session.DrainSerial(hil::timeouts::bootBanner);
        (void)session.WaitForPrompt(hil::timeouts::prompt);

        std::cout << "[HIL] Bridge connected\n";
    }
    catch (const std::exception& e)
    {
        hil::SerialLogger::Instance().Note("ERROR", std::string{ "BEFORE_ALL: " } + e.what());
        std::cerr << "[HIL] BEFORE_ALL failed: " << e.what() << '\n';
        throw;
    }
}

HOOK_AFTER_ALL(.name = "Shutdown HIL")
{
    hil::SerialLogger::Instance().Note("HOOK", "AFTER_ALL");
    hil::SerialLogger::Instance().CloseSession();
}

HOOK_BEFORE_SCENARIO()
{
    hil::SerialLogger::Instance().BeginScenario();
    auto& session = hil::BridgeSession::Instance();
    (void)session.WaitForPrompt(hil::timeouts::promptShort);
    (void)session.SendSerial("stop", hil::timeouts::cleanup);
    (void)session.SendSerial("can_stop", hil::timeouts::cleanup);
    (void)session.WaitForPrompt(hil::timeouts::cleanup);
}

HOOK_AFTER_SCENARIO()
{
    auto& session = hil::BridgeSession::Instance();
    (void)session.DrainSerial(hil::timeouts::cleanup);
    (void)session.WaitForPrompt(hil::timeouts::promptShort);
    hil::SerialLogger::Instance().EndScenario(true);
}
