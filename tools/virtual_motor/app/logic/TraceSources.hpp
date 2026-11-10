#pragma once

#include "tools/virtual_motor/app/logic/CaptureAssembly.hpp"
#include "tools/virtual_motor/app/logic/TraceHistory.hpp"
#include <cstddef>
#include <cstdint>

namespace virtual_motor::app
{
    class TraceSource
    {
    public:
        virtual std::size_t Count() const = 0;
        virtual double Time(std::size_t index) const = 0;
        virtual float Minimum(std::size_t index, uint8_t signal) const = 0;
        virtual float Maximum(std::size_t index, uint8_t signal) const = 0;
        virtual bool BreakBefore(std::size_t index) const = 0;

        std::size_t FirstAtOrAfter(double time) const;

    protected:
        ~TraceSource() = default;
    };

    class HistoryTrace
        : public TraceSource
    {
    public:
        HistoryTrace(const TraceHistory& history, uint32_t controlRateHz);

        std::size_t Count() const override;
        double Time(std::size_t index) const override;
        float Minimum(std::size_t index, uint8_t signal) const override;
        float Maximum(std::size_t index, uint8_t signal) const override;
        bool BreakBefore(std::size_t index) const override;

    private:
        const TraceHistory& history;
        uint32_t controlRateHz;
    };

    class CaptureTrace
        : public TraceSource
    {
    public:
        CaptureTrace(const Capture& capture, uint32_t controlRateHz);

        std::size_t Count() const override;
        double Time(std::size_t index) const override;
        float Minimum(std::size_t index, uint8_t signal) const override;
        float Maximum(std::size_t index, uint8_t signal) const override;
        bool BreakBefore(std::size_t index) const override;

    private:
        const Capture& capture;
        uint32_t controlRateHz;
    };
}
