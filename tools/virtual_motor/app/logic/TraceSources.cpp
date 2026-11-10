#include "tools/virtual_motor/app/logic/TraceSources.hpp"
#include "tools/virtual_motor/app/logic/CaptureCsv.hpp"
#include <numeric>

namespace virtual_motor::app
{
    std::size_t TraceSource::FirstAtOrAfter(double time) const
    {
        std::size_t low{ 0 };
        std::size_t high{ Count() };
        while (low != high)
        {
            const auto middle = std::midpoint(low, high);
            if (Time(middle) < time)
                low = middle + 1;
            else
                high = middle;
        }
        return low;
    }

    HistoryTrace::HistoryTrace(const TraceHistory& history, uint32_t controlRateHz)
        : history(history)
        , controlRateHz(controlRateHz)
    {}

    std::size_t HistoryTrace::Count() const
    {
        return history.Count();
    }

    double HistoryTrace::Time(std::size_t index) const
    {
        return controlRateHz == 0 ? 0.0 : static_cast<double>(history.At(index).firstPeriod) / controlRateHz;
    }

    float HistoryTrace::Minimum(std::size_t index, uint8_t signal) const
    {
        return history.At(index).minimum[signal];
    }

    float HistoryTrace::Maximum(std::size_t index, uint8_t signal) const
    {
        return history.At(index).maximum[signal];
    }

    bool HistoryTrace::BreakBefore(std::size_t index) const
    {
        return history.At(index).afterGap;
    }

    CaptureTrace::CaptureTrace(const Capture& capture, uint32_t controlRateHz)
        : capture(capture)
        , controlRateHz(controlRateHz)
    {}

    std::size_t CaptureTrace::Count() const
    {
        return capture.periods.size();
    }

    double CaptureTrace::Time(std::size_t index) const
    {
        return CaptureTime(capture, index, controlRateHz);
    }

    float CaptureTrace::Minimum(std::size_t index, uint8_t signal) const
    {
        return capture.periods[index][signal];
    }

    float CaptureTrace::Maximum(std::size_t index, uint8_t signal) const
    {
        return capture.periods[index][signal];
    }

    bool CaptureTrace::BreakBefore(std::size_t) const
    {
        return false;
    }
}
