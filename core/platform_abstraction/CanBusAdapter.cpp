#include "core/platform_abstraction/CanBusAdapter.hpp"

namespace application
{
    void CanBusAdapter::ErrorCounters::Record(CanError error)
    {
        auto& counter = counters.at(IndexOf(error));
        counter = Increment(counter);
        total = Increment(total);
    }

    void CanBusAdapter::ErrorCounters::Reset()
    {
        counters = std::array<uint32_t, errorClasses>{};
        total = 0;
    }

    uint32_t CanBusAdapter::ErrorCounters::Count(CanError error) const
    {
        return counters.at(IndexOf(error));
    }

    uint32_t CanBusAdapter::ErrorCounters::Total() const
    {
        return total;
    }

    std::size_t CanBusAdapter::ErrorCounters::IndexOf(CanError error)
    {
        const auto index = static_cast<std::size_t>(error);

        return index < errorClasses ? index : errorClasses - 1;
    }

    uint32_t CanBusAdapter::ErrorCounters::Increment(uint32_t counter)
    {
        return counter == saturated ? counter : counter + 1;
    }

    const CanBusAdapter::ErrorCounters& CanBusAdapter::ErrorStatistics() const
    {
        return errorCounters;
    }

    void CanBusAdapter::ResetErrorStatistics()
    {
        errorCounters.Reset();
    }

    void CanBusAdapter::RecordError(CanError error)
    {
        errorCounters.Record(error);
    }
}
