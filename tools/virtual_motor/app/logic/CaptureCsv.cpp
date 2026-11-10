#include "tools/virtual_motor/app/logic/CaptureCsv.hpp"
#include "tools/virtual_motor/app/logic/TraceSignals.hpp"
#include <format>

namespace virtual_motor::app
{
    double CaptureTime(const Capture& capture, std::size_t index, uint32_t controlRateHz)
    {
        const auto fromTrigger = static_cast<double>(index) - static_cast<double>(capture.preTrigger);
        return controlRateHz == 0 ? 0.0 : fromTrigger / controlRateHz;
    }

    std::string CaptureCsv(const Capture& capture, uint32_t controlRateHz)
    {
        std::string csv{ "time [s],control period" };
        for (uint8_t signal = 0; signal != port::signalCount; ++signal)
            csv += "," + SignalLabel(signal);
        csv += "\n";

        for (std::size_t index = 0; index != capture.periods.size(); ++index)
        {
            csv += std::format("{:.9g},{}", CaptureTime(capture, index, controlRateHz), capture.firstPeriod + index);
            for (const auto value : capture.periods[index])
                csv += std::format(",{:.7g}", value);
            csv += "\n";
        }
        return csv;
    }
}
