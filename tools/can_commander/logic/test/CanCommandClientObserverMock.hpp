#pragma once

#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "gmock/gmock.h"

namespace tool
{
    class CanCommandClientObserverMock : public CanCommandClientObserver
    {
    public:
        using CanCommandClientObserver::CanCommandClientObserver;

        MOCK_METHOD(void, OnConnectionChanged, (bool connected), (override));
        MOCK_METHOD(void, OnAdapterError, (infra::BoundedConstString message), (override));
        MOCK_METHOD(void, OnFrameLog, (bool transmitted, uint32_t id, const services::CanFrame& data), (override));
        MOCK_METHOD(void, OnNodeOnlineChanged, (uint16_t nodeId, bool online), (override));
        MOCK_METHOD(void, OnBusyChanged, (uint16_t nodeId, bool busy), (override));
        MOCK_METHOD(void, OnCommandTimeout, (uint16_t nodeId, uint8_t commandType), (override));
        MOCK_METHOD(void, OnCommandAck, (uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status), (override));
        MOCK_METHOD(void, OnCommandRejected, (uint16_t nodeId, uint8_t commandType, can::FocMotorCategoryError error), (override));
        MOCK_METHOD(void, OnContractVersion, (uint16_t nodeId, uint8_t major, uint8_t minor, bool compatible), (override));
        MOCK_METHOD(void, OnControlModeAcknowledged, (uint16_t nodeId, can::FocMotorMode activeMode), (override));
        MOCK_METHOD(void, OnMotorStatusReceived, (uint16_t nodeId, FocMotorState state, FocFaultCode fault), (override));
        MOCK_METHOD(void, OnCurrentStreamReceived, (uint16_t nodeId, uint8_t sequence, float idCurrent, float iqCurrent, float position), (override));
        MOCK_METHOD(void, OnSpeedPositionReceived, (uint16_t nodeId, float speed, float position), (override));
        MOCK_METHOD(void, OnFaultEventReceived, (uint16_t nodeId, FocFaultCode fault), (override));
        MOCK_METHOD(void, OnElectricalParametersReceived, (uint16_t nodeId, float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs), (override));
        MOCK_METHOD(void, OnAlgorithmsReceived, (uint16_t nodeId, const can::FocAlgorithms& algorithms), (override));
        MOCK_METHOD(void, OnParameterReceived, (uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value), (override));
    };
}
