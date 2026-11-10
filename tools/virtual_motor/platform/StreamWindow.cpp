#include "tools/virtual_motor/platform/StreamWindow.hpp"
#include <algorithm>

namespace virtual_motor
{
    void StreamWindow::Begin()
    {
        sequence = 0;
        collected = 0;
    }

    std::optional<port::Window> StreamWindow::Add(uint64_t period, const SignalSample& sample)
    {
        if (collected == 0)
        {
            window.firstPeriod = period;
            window.minimum = sample;
            window.maximum = sample;
        }
        else
            for (std::size_t signal = 0; signal != sample.size(); ++signal)
            {
                window.minimum[signal] = std::min(window.minimum[signal], sample[signal]);
                window.maximum[signal] = std::max(window.maximum[signal], sample[signal]);
            }

        if (++collected != periodsPerWindow)
            return std::nullopt;

        collected = 0;
        window.periods = periodsPerWindow;
        window.sequence = sequence++;
        return window;
    }
}
