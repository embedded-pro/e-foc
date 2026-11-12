#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"

namespace virtual_motor
{
    class InstrumentReports
    {
    public:
        virtual void ReportWindow(const port::Window& window) = 0;
        virtual void ReportCaptureEvent(const port::CaptureEvent& event) = 0;
        virtual void ReportCaptureChunk(const port::CaptureChunk& chunk) = 0;
        virtual void ReportProfileState(const port::ProfileState& state) = 0;

    protected:
        ~InstrumentReports() = default;
    };
}
