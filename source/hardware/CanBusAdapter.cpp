#include "source/hardware/CanBusAdapter.hpp"

namespace infra
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const application::CanBusAdapter::CanError& error)
    {
        using enum application::CanBusAdapter::CanError;

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
            default:
                stream << "unknown";
                break;
        }

        return stream;
    }
}
