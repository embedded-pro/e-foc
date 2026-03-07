#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "source/services/can_protocol/client/CanProtocolClient.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/CanFrameTransport.hpp"
#include <cstdint>

namespace services
{
    class CanProtocolClientImpl
        : public CanProtocolClient
    {
    public:
        CanProtocolClientImpl(hal::Can& can, const Config& config);

        CanProtocolClientImpl(const CanProtocolClientImpl&) = delete;
        CanProtocolClientImpl& operator=(const CanProtocolClientImpl&) = delete;
        CanProtocolClientImpl(CanProtocolClientImpl&&) = delete;
        CanProtocolClientImpl& operator=(CanProtocolClientImpl&&) = delete;

        void SetNodeId(uint16_t nodeId) override;
        uint16_t NodeId() const override;

        void SendStartMotor(const infra::Function<void()>& onDone) override;
        void SendStopMotor(const infra::Function<void()>& onDone) override;
        void SendEmergencyStop(const infra::Function<void()>& onDone) override;
        void SendSetControlMode(CanControlMode mode, const infra::Function<void()>& onDone) override;
        void SendSetTorqueSetpoint(float idCurrent, float iqCurrent, const infra::Function<void()>& onDone) override;
        void SendSetSpeedSetpoint(float speedRadPerSec, const infra::Function<void()>& onDone) override;
        void SendSetPositionSetpoint(float positionRad, const infra::Function<void()>& onDone) override;

        void SendSetCurrentIdPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) override;
        void SendSetCurrentIqPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) override;
        void SendSetSpeedPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) override;
        void SendSetPositionPid(float kp, float ki, float kd, const infra::Function<void()>& onDone) override;

        void SendSetSupplyVoltage(float volts, const infra::Function<void()>& onDone) override;
        void SendSetMaxCurrent(float amps, const infra::Function<void()>& onDone) override;

        void RequestData(DataRequestFlags flags, const infra::Function<void()>& onDone) override;

    private:
        void ProcessReceivedMessage(hal::Can::Id id, const hal::Can::Message& data);

        void HandleTelemetry(CanMessageType type, const hal::Can::Message& data) const;
        void HandleMotorStatusTelemetry(const hal::Can::Message& data) const;
        void HandleCurrentMeasurementTelemetry(const hal::Can::Message& data) const;
        void HandleSpeedPositionTelemetry(const hal::Can::Message& data) const;
        void HandleBusVoltageTelemetry(const hal::Can::Message& data) const;
        void HandleFaultEventTelemetry(const hal::Can::Message& data) const;

        void HandleSystemMessage(CanMessageType type, const hal::Can::Message& data) const;
        void HandleCommandAck(const hal::Can::Message& data) const;

        void SendPidGains(CanMessageType type, float kp, float ki, float kd, const infra::Function<void()>& onDone);

        uint8_t NextSequence();

        CanFrameTransport transport;
        uint8_t sequenceCounter = 0;
    };
}
