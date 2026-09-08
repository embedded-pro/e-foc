#include "integration_tests/support/interactor/hardware/HardwareInteractor.hpp"
#include "integration_tests/support/interactor/hardware/GdbFlasher.hpp"
#include <iostream>

namespace hil
{
    HardwareInteractor::HardwareInteractor(const BridgeConfig& config)
    {
        SetBridgeConfig(config);
    }

    void HardwareInteractor::Setup()
    {
        SerialLogger::Instance().OpenSession();
        const auto& config = GetBridgeConfig();
        GdbFlasher{ config }.Flash();
        auto& session = BridgeSession::Instance();
        (void)session.DrainSerial(timeouts::bootBanner);
        (void)session.WaitForPrompt(timeouts::prompt);
        std::cout << "[HIL] Bridge connected\n";
    }

    void HardwareInteractor::Teardown()
    {
        SerialLogger::Instance().CloseSession();
    }

    void HardwareInteractor::BeforeScenario()
    {
        SerialLogger::Instance().BeginScenario();
        auto& session = BridgeSession::Instance();
        (void)session.WaitForPrompt(timeouts::promptShort);
        (void)session.SendSerial("stop", timeouts::cleanup);
        (void)session.SendSerial("can_stop", timeouts::cleanup);
        (void)session.WaitForPrompt(timeouts::cleanup);
    }

    void HardwareInteractor::AfterScenario()
    {
        auto& session = BridgeSession::Instance();
        (void)session.DrainSerial(timeouts::cleanup);
        (void)session.WaitForPrompt(timeouts::promptShort);
        SerialLogger::Instance().EndScenario(true);
    }

    bool HardwareInteractor::SendCommand(const std::string& command, std::chrono::milliseconds timeout)
    {
        return BridgeSession::Instance().SendSerial(command, timeout);
    }

    bool HardwareInteractor::DrainSerial(std::chrono::milliseconds timeout)
    {
        return BridgeSession::Instance().DrainSerial(timeout);
    }

    bool HardwareInteractor::WaitForPrompt(std::chrono::milliseconds timeout)
    {
        return BridgeSession::Instance().WaitForPrompt(timeout);
    }

    void HardwareInteractor::ClearSerialLines()
    {
        BridgeSession::Instance().ClearSerialLines();
    }

    const std::vector<std::string>& HardwareInteractor::SerialLines() const
    {
        return BridgeSession::Instance().SerialLines();
    }

    const std::string& HardwareInteractor::LastSerialLine() const
    {
        return BridgeSession::Instance().LastSerialLine();
    }

    std::chrono::milliseconds HardwareInteractor::LastSerialDuration() const
    {
        return BridgeSession::Instance().LastSerialDuration();
    }

    void HardwareInteractor::ClearCanFrames()
    {
        BridgeSession::Instance().ClearCanFrames();
    }

    bool HardwareInteractor::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        return BridgeSession::Instance().SendCanFrame(id, message, timeout);
    }

    bool HardwareInteractor::WaitForCanFrame(hal::Can::Id expectedId, hal::Can::Message& out,
        std::chrono::milliseconds timeout,
        std::chrono::steady_clock::time_point referenceTime,
        std::chrono::milliseconds& elapsed)
    {
        BridgeSession::CanFrame frame;
        const bool ok = BridgeSession::Instance().WaitForCanFrame(
            expectedId, frame, timeout, referenceTime, elapsed);
        if (ok)
            out = frame.message;
        return ok;
    }
}
