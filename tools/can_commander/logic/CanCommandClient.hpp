#pragma once

#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include <cstdint>
#include <map>
#include <set>

namespace tool
{
    using FocMotorState = can::FocMotorState;
    using FocFaultCode = can::FocFaultCode;

    class CanCommandClient;

    class CanCommandClientObserver
        : public infra::SingleObserver<CanCommandClientObserver, CanCommandClient>
    {
    public:
        using infra::SingleObserver<CanCommandClientObserver, CanCommandClient>::SingleObserver;

        // The bus adapter opened or closed.
        virtual void OnConnectionChanged(bool connected) = 0;
        virtual void OnAdapterError(infra::BoundedConstString message) = 0;
        virtual void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) = 0;

        // A node's heartbeat appeared or timed out; the bus stays open either way.
        virtual void OnNodeOnlineChanged(uint16_t nodeId, bool online) = 0;
        virtual void OnBusyChanged(uint16_t nodeId, bool busy) = 0;
        virtual void OnCommandTimeout(uint16_t nodeId) = 0;
        virtual void OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status) = 0;
        virtual void OnCommandRejected(uint16_t nodeId, uint8_t commandType, can::FocMotorCategoryError error) = 0;
        virtual void OnContractVersion(uint16_t nodeId, uint8_t major, uint8_t minor, bool compatible) = 0;
        virtual void OnControlModeAcknowledged(uint16_t nodeId, can::FocMotorMode activeMode) = 0;

        virtual void OnMotorStatusReceived(uint16_t nodeId, FocMotorState state, FocFaultCode fault) = 0;
        virtual void OnCurrentMeasurementReceived(uint16_t nodeId, float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedPositionReceived(uint16_t nodeId, float speed, float position) = 0;
        virtual void OnBusVoltageReceived(uint16_t nodeId, float voltage) = 0;
        virtual void OnFaultEventReceived(uint16_t nodeId, FocFaultCode fault) = 0;
        virtual void OnElectricalParametersReceived(uint16_t nodeId, float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs) = 0;
        virtual void OnMechanicalParametersReceived(uint16_t nodeId, float frictionNmSPerRad, float inertiaKgM2) = 0;
    };

    // One bus, many motor controllers: every command names its node, and every report says which
    // node it came from. Each node tracks the last command it was sent until that command is
    // acknowledged, rejected or times out; a newer command, such as an emergency stop, takes over.
    class CanCommandClient
        : public infra::Subject<CanCommandClientObserver>
        , private services::CanProtocolClientObserver
        , private can::FocMotorCategoryClientObserver
        , private CanBusAdapterObserver
    {
    public:
        static constexpr infra::Duration commandTimeout{ std::chrono::seconds(2) };

        explicit CanCommandClient(CanBusAdapter& adapter);
        ~CanCommandClient();

        bool IsBusy(uint16_t nodeId) const;
        bool IsOnline(uint16_t nodeId) const;
        std::set<uint16_t> OnlineNodes() const;
        can::ContractCompatibility Compatibility(uint16_t nodeId) const;

        // Each Send returns false when the frame was refused locally (incompatible contract or a
        // full transmit queue); nothing reached the bus and no acknowledgement will follow.
        bool SendStartMotor(uint16_t nodeId);
        bool SendStopMotor(uint16_t nodeId);
        bool SendEmergencyStop(uint16_t nodeId);
        bool SendSetControlMode(uint16_t nodeId, can::FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent);
        bool SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec);
        bool SendSetPositionSetpoint(uint16_t nodeId, float positionRad);

        bool SendSetCurrentBandwidth(uint16_t nodeId, float bandwidth);
        bool SendSetSpeedBandwidth(uint16_t nodeId, float bandwidth);
        bool SendSetPositionBandwidth(uint16_t nodeId, float bandwidth);

        bool RequestData(uint16_t nodeId);

    private:
        struct NodeCommand
        {
            bool busy{ false };
            uint8_t pendingCommand{ 0 };
            infra::TimerSingleShot timeout;
        };

        void MarkBusy(uint16_t nodeId, uint8_t commandType);
        void ClearBusy(uint16_t nodeId);
        void CompleteCommand(uint16_t nodeId, uint8_t commandType);
        void MarkOnline(uint16_t nodeId);
        void MarkOffline(uint16_t nodeId);

        template<class Send>
        bool Tracked(uint16_t nodeId, uint8_t commandType, Send send);

        // CanProtocolClientObserver
        void OnServerOnline(uint16_t nodeId) override;
        void OnServerOffline(uint16_t nodeId) override;
        void OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType) override;

        // FocMotorCategoryClientObserver
        void OnSelectControlModeResponse(uint16_t nodeId, can::FocMotorMode activeMode) override;
        void OnCategoryError(uint16_t nodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode) override;
        void OnTelemetryStatus(uint16_t nodeId, const hal::Can::Message& msg) override;
        void OnTelemetryElectrical(uint16_t nodeId, const hal::Can::Message& msg) override;
        void OnContractVersionResponse(uint16_t nodeId, uint8_t major, uint8_t minor) override;
        void OnElectricalParamsResponse(uint16_t nodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs) override;
        void OnMechanicalParamsResponse(uint16_t nodeId, foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia) override;

        // CanBusAdapterObserver
        void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        bool HandleCommandAck(uint32_t id, const CanFrame& data);
        void DecodeTelemetryStatus(uint16_t nodeId, const hal::Can::Message& msg) const;
        void DecodeTelemetryElectrical(uint16_t nodeId, const hal::Can::Message& msg) const;

        can::FocMotorCanClient focClient;
        std::map<uint16_t, NodeCommand> commands;
        std::set<uint16_t> onlineNodes;
    };
}
