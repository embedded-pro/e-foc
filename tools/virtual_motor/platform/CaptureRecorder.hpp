#pragma once

#include "tools/virtual_motor/platform/InstrumentReports.hpp"
#include "tools/virtual_motor/platform/TriggerWatch.hpp"
#include <deque>
#include <optional>
#include <vector>

namespace virtual_motor
{
    class CaptureRecorder
    {
    public:
        void Report(InstrumentReports& reports);

        std::optional<port::RefusalReason> Arm(const port::CaptureRequest& request, uint64_t period);
        void Cancel(uint64_t period);
        bool Active() const;

        void Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection);

    private:
        enum class State : uint8_t
        {
            idle,
            armed,
            recording,
        };

        void Event(port::CaptureEventKind event, uint64_t period);
        void Triggered(uint64_t period, const port::Sample& sample);
        void Deliver();

        InstrumentReports* reports{ nullptr };
        TriggerWatch watch;
        State state{ State::idle };
        uint16_t number{ 0 };
        std::size_t preTarget{ 0 };
        std::size_t postTarget{ 0 };
        std::size_t preTrigger{ 0 };
        uint64_t firstPeriod{ 0 };
        std::deque<port::Sample> before;
        std::vector<port::Sample> recorded;
    };
}
