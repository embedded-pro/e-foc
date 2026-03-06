#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Observer.hpp"
#include "source/services/can_protocol/CanCategoryHandler.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/CanProtocolHandlerObserver.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolHandler
        : public infra::Subject<CanProtocolHandlerObserver>
    {
    public:
        struct Config
        {
            uint16_t nodeId = 1;
            uint16_t maxMessagesPerSecond = 500;
        };

        virtual void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data) = 0;
        virtual void StartListening() = 0;

        virtual void SendMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault) = 0;
        virtual void SendCurrentMeasurement(float idCurrent, float iqCurrent) = 0;
        virtual void SendSpeedPosition(float speed, float position) = 0;
        virtual void SendBusVoltage(float voltage) = 0;
        virtual void SendHeartbeat() = 0;
        virtual void SendFaultEvent(CanFaultCode fault) = 0;
        virtual void SendCommandAck(CanMessageType commandType, CanAckStatus status) = 0;

        virtual void ResetRateCounter() = 0;
        virtual bool IsRateLimited() const = 0;

    protected:
        CanProtocolHandler() = default;
        CanProtocolHandler(const CanProtocolHandler&) = delete;
        CanProtocolHandler& operator=(const CanProtocolHandler&) = delete;
        ~CanProtocolHandler() = default;
    };
}
