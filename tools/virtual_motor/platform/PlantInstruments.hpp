#pragma once

#include "tools/virtual_motor/platform/CaptureRecorder.hpp"
#include "tools/virtual_motor/platform/InstrumentReports.hpp"
#include "tools/virtual_motor/platform/ProfileRunner.hpp"
#include "tools/virtual_motor/platform/StreamWindow.hpp"
#include <optional>

namespace virtual_motor
{
    class PlantInstruments
    {
    public:
        explicit PlantInstruments(PlantDisturbances& disturbances);

        void Attach(InstrumentReports& reports);
        void Detach(uint64_t period);

        void Stream(bool on);
        std::optional<port::RefusalReason> Capture(const port::CaptureRequest& request, uint64_t period);
        void CancelCapture(uint64_t period);
        std::optional<port::RefusalReason> RunProfile(const port::ProfileRequest& profile);
        void StopProfile();

        bool Active() const;
        void Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection);

    private:
        class Forward
            : public InstrumentReports
        {
        public:
            void ReportWindow(const port::Window& window) override;
            void ReportCaptureEvent(const port::CaptureEvent& event) override;
            void ReportCaptureChunk(const port::CaptureChunk& chunk) override;
            void ReportProfileState(const port::ProfileState& state) override;

            InstrumentReports* to{ nullptr };
        };

        Forward forward;
        StreamWindow window;
        bool streaming{ false };
        CaptureRecorder capture;
        ProfileRunner profile;
    };
}
