#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"

namespace hil
{
    HilFixture::HilFixture()
        : session(BridgeSession::Instance())
    {}

    HilFixture::~HilFixture() = default;

    bool HilFixture::SendCommand(const std::string& command,
        std::chrono::milliseconds timeout)
    {
        const bool received = session.SendSerial(command, timeout);
        allLines = session.SerialLines();
        lastResponse = session.LastSerialLine();
        lastCommandDuration = session.LastSerialDuration();
        return received;
    }

    bool HilFixture::DrainLines(std::chrono::milliseconds timeout)
    {
        const bool received = session.DrainSerial(timeout);
        allLines = session.SerialLines();
        lastResponse = session.LastSerialLine();
        lastCommandDuration = session.LastSerialDuration();
        return received;
    }

    void HilFixture::StartCanCapture()
    {
        session.ClearCanFrames();
        canReference = {};
    }

    void HilFixture::MarkCanReference()
    {
        canReference = std::chrono::steady_clock::now();
    }

    bool HilFixture::SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
        std::chrono::milliseconds timeout)
    {
        return session.SendCanFrame(id, message, timeout);
    }

    bool HilFixture::WaitForCanFrame(hal::Can::Id expectedId,
        hal::Can::Message& outPayload,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds& outElapsed)
    {
        BridgeSession::CanFrame frame;
        const bool ok = session.WaitForCanFrame(
            expectedId, frame, timeout, canReference, outElapsed);
        if (ok)
            outPayload = frame.message;
        return ok;
    }
}
