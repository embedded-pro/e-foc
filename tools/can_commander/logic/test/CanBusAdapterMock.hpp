#pragma once

#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include "gmock/gmock.h"

namespace tool
{
    class CanBusAdapterMock : public CanBusAdapter
    {
    public:
        MOCK_METHOD(bool, Connect, (infra::BoundedConstString interfaceName, uint32_t bitrate), (override));
        MOCK_METHOD(void, Disconnect, (), (override));
        MOCK_METHOD(bool, IsConnected, (), (const, override));
        MOCK_METHOD(void, SendData, (Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (const infra::Function<void(Id id, const Message& data)>& receivedAction), (override));
        MOCK_METHOD(int, FileDescriptor, (), (const, override));
        MOCK_METHOD(void, ProcessReadEvent, (), (override));
    };
}
