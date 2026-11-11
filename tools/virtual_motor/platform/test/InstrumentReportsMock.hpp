#pragma once

#include "tools/virtual_motor/platform/InstrumentReports.hpp"
#include "gmock/gmock.h"

namespace virtual_motor
{
    class InstrumentReportsMock
        : public InstrumentReports
    {
    public:
        MOCK_METHOD(void, ReportWindow, (const port::Window& window), (override));
        MOCK_METHOD(void, ReportCaptureEvent, (const port::CaptureEvent& event), (override));
        MOCK_METHOD(void, ReportCaptureChunk, (const port::CaptureChunk& chunk), (override));
        MOCK_METHOD(void, ReportProfileState, (const port::ProfileState& state), (override));
    };
}
