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

    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, CanBusAdapter::CanError error)
    {
        using enum CanBusAdapter::CanError;
        switch (error)
        {
            case busOff:
                stream << "bus off";
                break;
            case errorPassive:
                stream << "error passive";
                break;
            case errorWarning:
                stream << "error warning";
                break;
            case messageLost:
                stream << "message lost";
                break;
            case rxBufferOverflow:
                stream << "rx buffer overflow";
                break;
            case ackError:
                stream << "ack error";
                break;
            case stuffError:
                stream << "stuff error";
                break;
            case formError:
                stream << "form error";
                break;
            case crcError:
                stream << "crc error";
                break;
            case bit0Error:
                stream << "bit0 error";
                break;
            case bit1Error:
                stream << "bit1 error";
                break;
            case other:
                stream << "other";
                break;
            default:
                stream << "unknown";
                break;
        }

        return stream;
    }
}
