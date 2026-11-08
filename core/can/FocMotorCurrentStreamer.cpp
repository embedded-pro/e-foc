#include "core/can/FocMotorCurrentStreamer.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include <algorithm>
#include <chrono>

namespace can
{
    namespace
    {
        constexpr auto tickInterval{ std::chrono::milliseconds(1) };
    }

    bool FocMotorCurrentStreamer::IsStreamFrame(hal::Can::Id id)
    {
        return id.Is29BitId() && services::ExtractCanCategory(id.Get29BitId()) == focMotorCategoryId &&
               services::ExtractCanMessageType(id.Get29BitId()) == focTelemetryCurrentsId;
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

            ++framesWaiting;
            const auto position = foc::Radians{ math::WrapToPi(sample->position.Value()) };
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
