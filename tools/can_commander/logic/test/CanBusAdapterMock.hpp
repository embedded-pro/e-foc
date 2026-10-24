#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "gmock/gmock.h"

namespace tool
{
    class CanBusAdapterMock : public services::CanBusAdapter
    {
    public:
        MOCK_METHOD(bool, Connect, (infra::BoundedConstString interfaceName, uint32_t bitrate), (override));
        MOCK_METHOD(void, Disconnect, (), (override));
        MOCK_METHOD(bool, IsConnected, (), (const, override));
        MOCK_METHOD(void, SendData, (Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (const infra::Function<void(Id id, const Message& data)>& receivedAction), (override));
        MOCK_METHOD(intptr_t, FileDescriptor, (), (const, override));
        MOCK_METHOD(void, ProcessReadEvent, (), (override));
        MOCK_METHOD(void, EnumerateInterfaces, (const infra::Function<void(infra::BoundedConstString)>& callback), (const, override));
        MOCK_METHOD(void, EnumerateMissingDrivers, (const infra::Function<void(infra::BoundedConstString reason)>& callback), (const, override));
    };
}
