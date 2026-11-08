#pragma once

#include "hal/interfaces/Can.hpp"

namespace can
{
    class UntracedStreamCan
        : public hal::Can
    {
    public:
        UntracedStreamCan(hal::Can& traced, hal::Can& untraced);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    private:
        hal::Can& traced;
        hal::Can& untraced;
    };
}
