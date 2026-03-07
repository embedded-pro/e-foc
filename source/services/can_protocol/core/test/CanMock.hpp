#pragma once

#include "hal/interfaces/Can.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class CanMock
        : public Can
    {
    public:
        MOCK_METHOD(void, SendData, (Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (const infra::Function<void(Id id, const Message& data)>& receivedAction), (override));
    };
}
