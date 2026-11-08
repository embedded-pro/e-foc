#include "core/services/leds/StatusLeds.hpp"

namespace services
{
    StatusLeds::StatusLeds(hal::GpioPin& green, hal::GpioPin& amber, hal::GpioPin& red, const LedStatusSource& source)
        : source{ source }
        , green{ green }
        , amber{ amber }
        , red{ red }
        , timer{ tickPeriod, [this]()
            {
                Refresh();
            } }
    {
        Refresh();
    }

    void StatusLeds::Refresh()
    {
        const auto patterns = PatternsFor(source.Status());
        green.Update(patterns.green);
        amber.Update(patterns.amber);
        red.Update(patterns.red);
    }
}
