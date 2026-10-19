#include "core/services/cli/SerialCommunicationMirror.hpp"

namespace services
{
    SerialCommunicationMirror::SerialCommunicationMirror(hal::SerialCommunication& primary, hal::SerialCommunication& secondary)
        : primary(primary)
        , secondary(secondary)
    {}

    void SerialCommunicationMirror::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        secondary.SendData(data, infra::emptyFunction);
        primary.SendData(data, actionOnCompletion);
    }

    void SerialCommunicationMirror::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        primary.ReceiveData(dataReceived);
        secondary.ReceiveData(dataReceived);
    }
}
