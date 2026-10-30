#include "core/can/UntracedStreamCan.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"

namespace can
{
    namespace
    {
        bool IsCurrentStream(hal::Can::Id id)
        {
            return id.Is29BitId() && services::ExtractCanCategory(id.Get29BitId()) == focMotorCategoryId &&
                   services::ExtractCanMessageType(id.Get29BitId()) == focTelemetryCurrentsId;
        }
    }

    UntracedStreamCan::UntracedStreamCan(hal::Can& traced, hal::Can& untraced)
        : traced(traced)
        , untraced(untraced)
    {}

    void UntracedStreamCan::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (IsCurrentStream(id))
            untraced.SendData(id, data, actionOnCompletion);
        else
            traced.SendData(id, data, actionOnCompletion);
    }

    void UntracedStreamCan::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        traced.ReceiveData(receivedAction);
    }
}
