#include "core/can/FocMotorCurrentStreamer.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <algorithm>
#include <chrono>

namespace can
{
    namespace
    {
        constexpr auto tickInterval{ std::chrono::milliseconds(1) };
    }

    FocMotorCurrentStreamer::FocMotorCurrentStreamer(services::CanProtocolServer& protocolServer, FocMotorCategoryServer& server,
        foc::CurrentStream& stream, const services::ConfigData& config, hal::Hertz controlFrequency)
        : services::CanProtocolServerObserver(protocolServer)
        , server(server)
        , stream(stream)
        , config(config)
        , controlFrequencyHz(controlFrequency.Value())
    {}

    void FocMotorCurrentStreamer::Online()
    {
        // The protocol server reports Online for every client heartbeat, not only when the client returns.
        if (timer.Armed())
            return;

        appliedRateHz.reset();
        ApplyConfiguredRate();
        timer.Start(tickInterval, [this]()
            {
                Tick();
            });
    }

    void FocMotorCurrentStreamer::Offline()
    {
        timer.Cancel();
        stream.Stop();
        appliedRateHz.reset();
    }

    void FocMotorCurrentStreamer::Tick()
    {
        ApplyConfiguredRate();
        SendWaitingSamples();
    }

    void FocMotorCurrentStreamer::ApplyConfiguredRate()
    {
        const auto rateHz = std::min(config.telemetryRateHz, focMaxTelemetryRateHz);
        if (appliedRateHz == rateHz)
            return;

        appliedRateHz = rateHz;
        if (rateHz == 0)
            stream.Stop();
        else
            stream.Start(std::max<uint32_t>(1, (controlFrequencyHz + rateHz / 2) / rateHz));
    }

    void FocMotorCurrentStreamer::SendWaitingSamples()
    {
        while (framesWaiting < maxFramesWaiting)
        {
            const auto sample = stream.Take();
            if (!sample)
                return;

            // A synchronous bus completes the frame inside the send, so the count goes up before it.
            ++framesWaiting;
            const auto position = foc::Radians{ foc::detail::PositionWithWrapAround(sample->position.Value()) };
            if (!server.BroadcastTelemetryCurrents(sample->sequence, sample->meanId, sample->meanIq, position, [this](bool)
                    {
                        OnFrameDone();
                    }))
                OnFrameDone();
        }
    }

    void FocMotorCurrentStreamer::OnFrameDone()
    {
        if (framesWaiting != 0)
            --framesWaiting;
    }
}
