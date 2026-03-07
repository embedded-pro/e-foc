#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "source/services/can_protocol/core/CanCategoryHandler.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/server/CanCategoryHandlers.hpp"
#include "source/services/can_protocol/server/CanProtocolServer.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class CanProtocolServerImpl
        : public CanProtocolServer
    {
    public:
        CanProtocolServerImpl(hal::Can& can, const Config& config);

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

        struct PendingFrame
        {
            hal::Can::Id id;
            hal::Can::Message data;
            infra::Function<void()> onDone;
        };

        void SendFrame(CanPriority priority, CanCategory category, CanMessageType type,
            const hal::Can::Message& data, const infra::Function<void()>& onDone);
        void SendNextQueued();

        hal::Can& can;
        Config config;
        uint16_t messageCountThisPeriod = 0;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;
        bool sendInProgress = false;
        infra::Function<void()> currentOnDone;
        infra::BoundedDeque<PendingFrame>::WithMaxSize<8> sendQueue;

        CanMotorControlHandler motorControlHandler;
        CanPidTuningHandler pidTuningHandler;
        CanSystemParametersHandler systemParametersHandler;
        CanSystemHandler systemHandler;
        std::array<CanCategoryHandler*, 4> handlers;
    };
}
