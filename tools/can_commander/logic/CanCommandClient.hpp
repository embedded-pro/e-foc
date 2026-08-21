#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/util/Observer.hpp"
#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include <cstdint>

namespace tool
{
    enum class FocMotorState : uint8_t
    {
        idle = 0,
        running = 1,
        fault = 2,
        calibrating = 3
    };

    enum class FocFaultCode : uint8_t
    {
        none = 0,
        overCurrent = 1,
        overVoltage = 2,
        underVoltage = 3,
        overTemperature = 4,
        sensorFault = 5
    };

    class CanCommandClient;

    class CanCommandClientObserver
        : public infra::SingleObserver<CanCommandClientObserver, CanCommandClient>
    {
    public:
        using infra::SingleObserver<CanCommandClientObserver, CanCommandClient>::SingleObserver;

        virtual void OnCommandTimeout() = 0;
        virtual void OnBusyChanged(bool busy) = 0;

        virtual void OnMotorStatusReceived(FocMotorState state, FocFaultCode fault) = 0;
        virtual void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedPositionReceived(float speed, float position) = 0;
        virtual void OnBusVoltageReceived(float voltage) = 0;
        virtual void OnFaultEventReceived(FocFaultCode fault) = 0;

        virtual void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) = 0;

        virtual void OnConnectionChanged(bool connected) = 0;
        virtual void OnAdapterError(infra::BoundedConstString message) = 0;

        virtual void OnControlModeAcknowledged(can::FocMotorMode activeMode) = 0;
        virtual void OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status) = 0;
    };

    class CanCommandClient
        : public infra::Subject<CanCommandClientObserver>
        , private services::CanProtocolClientObserver
        , private can::FocMotorCategoryClientObserver
        , private CanBusAdapterObserver
    {
    public:
        explicit CanCommandClient(CanBusAdapter& adapter);
        ~CanCommandClient();

        void SetNodeId(uint16_t nodeId);
        uint16_t NodeId() const;
        bool IsBusy() const;

        void SendStartMotor();
        void SendStopMotor();
        void SendEmergencyStop();
        void SendSetControlMode(can::FocMotorMode mode);
        void SendSetTorqueSetpoint(float iqCurrent);
        void SendSetSpeedSetpoint(float speedRadPerSec);
        void SendSetPositionSetpoint(float positionRad);

        void SendSetCurrentIdPid(float kp, float ki, float kd);
        void SendSetCurrentIqPid(float kp, float ki, float kd);
        void SendSetSpeedPid(float kp, float ki, float kd);
        void SendSetPositionPid(float kp, float ki, float kd);

        void RequestData() const;
        void HandleTimeout();

    private:
        void SetBusy(bool newBusy);

        // CanProtocolClientObserver
        void OnServerOnline(uint16_t nodeId) override;
        void OnServerOffline(uint16_t nodeId) override;
        void OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType) override;

        // FocMotorCategoryClientObserver
        void OnSelectControlModeResponse(can::FocMotorMode activeMode) override;
        void OnCategoryError(uint8_t originCommandId, can::FocMotorCategoryError errorCode) override;
        void OnTelemetryStatus(const hal::Can::Message& msg) override;
        void OnTelemetryElectrical(const hal::Can::Message& msg) override;

        // CanBusAdapterObserver
        void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        void HandleCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status);
        void DecodeTelemetryStatus(const hal::Can::Message& msg) const;
        void DecodeTelemetryElectrical(const hal::Can::Message& msg) const;

        uint16_t nodeId{ 1 };
        can::FocMotorCanClient focClient;
        bool busy{ false };
    };
}
