#pragma once

#include "source/tool/can_commander/logic/CanCommandClient.hpp"
#include "gmock/gmock.h"

namespace tool
{
    class CanCommandClientObserverMock : public CanCommandClientObserver
    {
    public:
        using CanCommandClientObserver::CanCommandClientObserver;

        MOCK_METHOD(void, OnCommandAckReceived, (services::CanCategory category, services::CanMessageType command, services::CanAckStatus status), (override));
        MOCK_METHOD(void, OnCommandTimeout, (), (override));
        MOCK_METHOD(void, OnBusyChanged, (bool busy), (override));
        MOCK_METHOD(void, OnMotorStatusReceived, (services::CanMotorState state, services::CanControlMode mode, services::CanFaultCode fault), (override));
        MOCK_METHOD(void, OnControlModeReceived, (services::CanControlMode mode), (override));
        MOCK_METHOD(void, OnCurrentMeasurementReceived, (float idCurrent, float iqCurrent), (override));
        MOCK_METHOD(void, OnSpeedPositionReceived, (float speed, float position), (override));
        MOCK_METHOD(void, OnBusVoltageReceived, (float voltage), (override));
        MOCK_METHOD(void, OnFaultEventReceived, (services::CanFaultCode fault), (override));
        MOCK_METHOD(void, OnHeartbeatReceived, (uint8_t protocolVersion), (override));
        MOCK_METHOD(void, OnFrameLog, (bool transmitted, uint32_t id, const CanFrame& data), (override));
        MOCK_METHOD(void, OnConnectionChanged, (bool connected), (override));
        MOCK_METHOD(void, OnAdapterError, (infra::BoundedConstString message), (override));
    };
}
