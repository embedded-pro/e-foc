#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedVector.hpp"
#include "source/services/can_protocol/core/CanCategoryHandler.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/CanFrameTransport.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/server/CanProtocolServer.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolServerImpl
        : public CanProtocolServer
    {
    public:
        using Handlers = infra::BoundedVector<CanCategoryHandler*>::WithMaxSize<8>;

        CanProtocolServerImpl(hal::Can& can, const Config& config, Handlers& handlers);

        using CanProtocolServer::SendCommandAck;

        void SendCommandAck(CanCategory category, CanMessageType commandType, CanAckStatus status, const infra::Function<void()>& onDone) override;

        void HandleDataRequest(DataRequestFlags flags) override;

        void ResetRateCounter() override;
        bool IsRateLimited() const override;

    private:
        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);
        bool CheckAndIncrementRate();
        bool ValidateSequence(uint8_t sequenceNumber);
        CanCategoryHandler* FindHandler(CanCategory category);

        void SendMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault);
        void SendCurrentMeasurement(float idCurrent, float iqCurrent);
        void SendSpeedPosition(float speed, float position);
        void SendBusVoltage(float voltage);
        void SendFaultEvent(CanFaultCode fault);

        Config config;
        CanFrameTransport transport;
        uint16_t messageCountThisPeriod = 0;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;

        Handlers& handlers;
    };
}
