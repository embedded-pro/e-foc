#include "tools/virtual_motor/app/logic/CaptureAssembly.hpp"
#include <format>

namespace virtual_motor::app
{
    void CaptureAssembly::Event(const port::CaptureEvent& event)
    {
        switch (event.event)
        {
            case port::CaptureEventKind::armed:
                current = event.capture;
                state = CaptureState::armed;
                placed.clear();
                received = 0;
                break;
            case port::CaptureEventKind::triggered:
                if (event.capture == current && state == CaptureState::armed)
                    state = CaptureState::triggered;
                break;
            case port::CaptureEventKind::cancelled:
                if (event.capture == current)
                    state = CaptureState::cancelled;
                break;
        }
    }

    bool CaptureAssembly::Chunk(const port::CaptureChunk& chunk)
    {
        if (!InFlight() || chunk.capture != current)
            return false;
        if (state != CaptureState::receiving)
            Start(chunk);

        for (std::size_t offset = 0; offset != chunk.periods.size(); ++offset)
        {
            const auto index = std::size_t{ chunk.index } + offset;
            if (index >= placed.size() || placed[index])
                continue;
            incoming.periods[index] = chunk.periods[offset];
            placed[index] = true;
            ++received;
        }

        if (received != placed.size())
            return false;
        state = CaptureState::complete;
        last = std::move(incoming);
        incoming = {};
        return true;
    }

    void CaptureAssembly::Interrupted()
    {
        if (InFlight())
            state = CaptureState::cancelled;
    }

    CaptureState CaptureAssembly::State() const
    {
        return state;
    }

    bool CaptureAssembly::InFlight() const
    {
        return state == CaptureState::armed || state == CaptureState::triggered || state == CaptureState::receiving;
    }

    std::size_t CaptureAssembly::Received() const
    {
        return received;
    }

    std::size_t CaptureAssembly::Expected() const
    {
        return placed.size();
    }

    const std::optional<Capture>& CaptureAssembly::Last() const
    {
        return last;
    }

    std::string CaptureAssembly::StatusText() const
    {
        switch (state)
        {
            case CaptureState::armed:
                return "Armed…";
            case CaptureState::triggered:
                return "Triggered";
            case CaptureState::receiving:
                return std::format("Receiving {} of {} periods", received, placed.size());
            case CaptureState::complete:
                return std::format("Complete: {} periods", last ? last->periods.size() : 0);
            case CaptureState::cancelled:
                return "Cancelled";
            default:
                return "No capture";
        }
    }

    void CaptureAssembly::Start(const port::CaptureChunk& chunk)
    {
        state = CaptureState::receiving;
        incoming = Capture{ chunk.capture, chunk.preTrigger, chunk.firstPeriod, std::vector<port::Sample>(chunk.length) };
        placed.assign(chunk.length, false);
        received = 0;
    }
}
