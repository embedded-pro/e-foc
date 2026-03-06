#pragma once

#include "hal/interfaces/Can.hpp"
#include "source/services/can_protocol/CanCategoryHandler.hpp"
#include "source/services/can_protocol/CanCategoryHandlers.hpp"
#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolDefinitions.hpp"
#include "source/services/can_protocol/CanProtocolHandler.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class CanProtocolHandlerImpl
        : public CanProtocolHandler
    {
    public:
        CanProtocolHandlerImpl(hal::Can& can, const Config& config);

        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data) override;
        void StartListening() override;

        void SendMotorStatus(CanMotorState state, CanControlMode mode, CanFaultCode fault) override;
        void SendCurrentMeasurement(float idCurrent, float iqCurrent) override;
        void SendSpeedPosition(float speed, float position) override;
        void SendBusVoltage(float voltage) override;
        void SendHeartbeat() override;
        void SendFaultEvent(CanFaultCode fault) override;
        void SendCommandAck(CanCategory category, CanMessageType commandType, CanAckStatus status) override;

        void ResetRateCounter() override;
        bool IsRateLimited() const override;

    private:
        bool CheckAndIncrementRate();
        bool ValidateSequence(uint8_t sequenceNumber);
        CanCategoryHandler* FindHandler(CanCategory category);
        void SendFrame(CanPriority priority, CanCategory category, CanMessageType type, const hal::Can::Message& data);

        hal::Can& can;
        Config config;
        uint16_t messageCountThisPeriod = 0;
        uint8_t lastSequenceNumber = 0;
        bool sequenceInitialized = false;

        CanMotorControlHandler motorControlHandler;
        CanPidTuningHandler pidTuningHandler;
        CanMotorParametersHandler motorParametersHandler;
        CanSystemParametersHandler systemParametersHandler;
        CanSystemHandler systemHandler;
        std::array<CanCategoryHandler*, 5> handlers;
    };
}
