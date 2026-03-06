#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include "gmock/gmock.h"

namespace tool
{
    class CanBusAdapterMock : public CanBusAdapter
    {
    public:
        MOCK_METHOD(bool, Connect, (infra::BoundedConstString interfaceName, uint32_t bitrate), (override));
        MOCK_METHOD(void, Disconnect, (), (override));
        MOCK_METHOD(bool, IsConnected, (), (const, override));
        MOCK_METHOD(bool, Send, (uint32_t id, const CanFrame& data), (override));
        MOCK_METHOD(int, FileDescriptor, (), (const, override));
        MOCK_METHOD(void, ProcessReadEvent, (), (override));
    };
}
