#include "tools/virtual_motor/app/logic/TraceHistory.hpp"

namespace virtual_motor::app
{
    TraceHistory::TraceHistory(std::size_t capacity)
        : capacity(capacity)
    {}

    void TraceHistory::Add(const port::Window& window)
    {
        Envelope envelope{ window.firstPeriod, window.periods, false, window.minimum, window.maximum, window.last };
        if (lastSequence.has_value() && window.sequence != *lastSequence + 1)
        {
            envelope.afterGap = true;
            dropped += window.sequence - *lastSequence - 1;
        }
        lastSequence = window.sequence;

        if (windows.size() < capacity)
            windows.push_back(envelope);
        else
        {
            windows[oldest] = envelope;
            oldest = (oldest + 1) % capacity;
        }
        ++revision;
    }

    void TraceHistory::Clear()
    {
        windows.clear();
        oldest = 0;
        lastSequence.reset();
        dropped = 0;
        ++revision;
    }

    std::size_t TraceHistory::Count() const
    {
        return windows.size();
    }

    const Envelope& TraceHistory::At(std::size_t index) const
    {
        return windows[(oldest + index) % windows.size()];
    }

    uint32_t TraceHistory::Dropped() const
    {
        return dropped;
    }

    uint64_t TraceHistory::Revision() const
    {
        return revision;
    }
}
