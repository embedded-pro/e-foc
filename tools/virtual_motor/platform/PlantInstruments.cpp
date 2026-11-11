#include "tools/virtual_motor/platform/PlantInstruments.hpp"

namespace virtual_motor
{
    PlantInstruments::PlantInstruments(PlantDisturbances& disturbances)
        : profile(disturbances)
    {
        capture.Report(forward);
        profile.Report(forward);
    }

    void PlantInstruments::Attach(InstrumentReports& reports)
    {
        forward.to = &reports;
    }

    void PlantInstruments::Detach(uint64_t period)
    {
        streaming = false;
        capture.Cancel(period);
        profile.Abandon();
        forward.to = nullptr;
    }

    void PlantInstruments::Stream(bool on)
    {
        if (on && !streaming)
            window.Begin();
        streaming = on;
    }

    std::optional<port::RefusalReason> PlantInstruments::Capture(const port::CaptureRequest& request, uint64_t period)
    {
        return capture.Arm(request, period);
    }

    void PlantInstruments::CancelCapture(uint64_t period)
    {
        capture.Cancel(period);
    }

    std::optional<port::RefusalReason> PlantInstruments::RunProfile(const port::ProfileRequest& request)
    {
        return profile.Run(request);
    }

    void PlantInstruments::StopProfile()
    {
        profile.Stop();
    }

    bool PlantInstruments::Active() const
    {
        return streaming || capture.Active() || profile.Active();
    }

    void PlantInstruments::Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection)
    {
        if (streaming)
            if (auto closed = window.Add(period, sample))
                forward.ReportWindow(*closed);

        capture.Observe(period, sample, leds, protection);
        profile.Observe(period, sample, leds, protection);
    }

    void PlantInstruments::Forward::ReportWindow(const port::Window& window)
    {
        if (to != nullptr)
            to->ReportWindow(window);
    }

    void PlantInstruments::Forward::ReportCaptureEvent(const port::CaptureEvent& event)
    {
        if (to != nullptr)
            to->ReportCaptureEvent(event);
    }

    void PlantInstruments::Forward::ReportCaptureChunk(const port::CaptureChunk& chunk)
    {
        if (to != nullptr)
            to->ReportCaptureChunk(chunk);
    }

    void PlantInstruments::Forward::ReportProfileState(const port::ProfileState& state)
    {
        if (to != nullptr)
            to->ReportProfileState(state);
    }
}
