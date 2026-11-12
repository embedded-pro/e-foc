#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "e-foc-protocol/client/FocMotorCanClient.hpp"
#include "e-foc-protocol/client/FocMotorCategoryClient.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include "tools/can_commander/logic/CommandTracker.hpp"
#include "tools/can_commander/logic/FirmwareIdentityCollector.hpp"
#include "tools/can_commander/logic/MotorCommands.hpp"
#include "tools/can_commander/logic/NodeData.hpp"
#include <cstdint>
#include <map>
#include <optional>
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

        virtual void OnConnectionChanged(bool connected) = 0;
        virtual void OnAdapterError(infra::BoundedConstString message) = 0;
        virtual void OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data) = 0;

        virtual void OnNodeOnlineChanged(uint16_t nodeId, bool online) = 0;
        virtual void OnBusyChanged(uint16_t nodeId, bool busy) = 0;
        virtual void OnCommandTimeout(uint16_t nodeId, uint8_t commandType) = 0;
        virtual void OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status) = 0;
        virtual void OnCommandRejected(uint16_t nodeId, uint8_t commandType, can::FocMotorCategoryError error) = 0;
        virtual void OnContractVersion(uint16_t nodeId, uint8_t major, uint8_t minor, bool compatible) = 0;
        virtual void OnControlModeAcknowledged(uint16_t nodeId, can::FocMotorMode activeMode) = 0;

        virtual void OnMotorStatusReceived(uint16_t nodeId, FocMotorState state, FocFaultCode fault) = 0;
        virtual void OnCurrentStreamReceived(uint16_t nodeId, uint8_t sequence, float idCurrent, float iqCurrent, float position) = 0;
        virtual void OnSpeedPositionReceived(uint16_t nodeId, float speed, float position) = 0;
        virtual void OnFaultEventReceived(uint16_t nodeId, FocFaultCode fault) = 0;
        virtual void OnAlgorithmsReceived(uint16_t nodeId, const can::FocAlgorithms& algorithms) = 0;
        virtual void OnParameterReceived(uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value) = 0;
        virtual void OnFirmwareIdentityReceived(uint16_t nodeId, const FirmwareIdentity& identity) = 0;
        virtual void OnFaultRecordReceived(uint16_t nodeId, const can::FocFaultRecord& record) = 0;
    };

    class CanCommandClient
        : public infra::Subject<CanCommandClientObserver>
        , private services::CanProtocolClientObserver
        , private can::FocMotorCategoryClientObserver
        , private services::CanBusAdapterObserver
    {
    public:
        static constexpr infra::Duration statusRefreshInterval{ std::chrono::seconds(1) };
        static constexpr uint8_t settingsContractMinor{ 1 };
        static constexpr uint8_t diagnosticsContractMinor{ 2 };
        static constexpr uint8_t supervisionContractMinor{ 4 };
        static constexpr uint8_t firmwareContractMinor{ 6 };
        static constexpr uint8_t limitsContractMinor{ 9 };
        static constexpr uint8_t targetWindowContractMinor{ 10 };
        static constexpr uint8_t travelLimitsContractMinor{ 11 };
        static constexpr uint8_t thermalContractMinor{ 13 };
        static constexpr uint8_t overTemperatureContractMinor{ 14 };
        static constexpr uint8_t busVoltageLimitsContractMinor{ 15 };
        static constexpr uint8_t quickStopContractMinor{ 16 };

        explicit CanCommandClient(services::CanBusAdapter& adapter);
        ~CanCommandClient();

        bool IsBusy(uint16_t nodeId) const;
        bool IsOnline(uint16_t nodeId) const;
        std::set<uint16_t> OnlineNodes() const;
        can::ContractCompatibility Compatibility(uint16_t nodeId) const;
        bool SupportsSettings(uint16_t nodeId) const;
        bool SupportsDiagnostics(uint16_t nodeId) const;
        bool SupportsSupervision(uint16_t nodeId) const;
        bool SupportsFirmwareIdentity(uint16_t nodeId) const;
        bool SupportsLimits(uint16_t nodeId) const;
        bool SupportsTargetWindow(uint16_t nodeId) const;
        bool SupportsTravelLimits(uint16_t nodeId) const;
        bool SupportsThermal(uint16_t nodeId) const;
        bool SupportsOverTemperature(uint16_t nodeId) const;
        bool SupportsBusVoltageLimits(uint16_t nodeId) const;
        bool SupportsQuickStop(uint16_t nodeId) const;
        static infra::Duration TimeoutFor(uint8_t commandType);

        bool SendStartMotor(uint16_t nodeId);
        bool SendStopMotor(uint16_t nodeId);
        bool SendEmergencyStop(uint16_t nodeId);
        bool SendQuickStop(uint16_t nodeId);
        bool SendSetControlMode(uint16_t nodeId, can::FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent);
        bool SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec);
        bool SendSetPositionSetpoint(uint16_t nodeId, float positionRad);

        bool SendSetCurrentBandwidth(uint16_t nodeId, float bandwidth);
        bool SendSetSpeedBandwidth(uint16_t nodeId, float bandwidth);
        bool SendSetPositionBandwidth(uint16_t nodeId, float bandwidth);

        bool SendClearFault(uint16_t nodeId);
        bool SendAlign(uint16_t nodeId);
        bool SendIdentifyElectrical(uint16_t nodeId);
        bool SendCalibrate(uint16_t nodeId);
        bool SendClearCalibration(uint16_t nodeId);
        bool SendClearDiagnostics(uint16_t nodeId);
        bool SendSetEncoderResolution(uint16_t nodeId, uint32_t countsPerRevolution);
        bool SendSelectAlgorithm(uint16_t nodeId, can::FocControlLoop loop, uint8_t algorithm);
        bool SendSetParameter(uint16_t nodeId, can::FocParameter parameter, float value);

        bool RequestData(uint16_t nodeId);
        void RequestSettings(uint16_t nodeId);
        void RequestReadings(uint16_t nodeId);
        void RequestStatus(uint16_t nodeId);
        void RequestFirmwareIdentity(uint16_t nodeId);
        void RequestFaultHistory(uint16_t nodeId);

        MotorCommands& Commands();
        NodeData& Data();
        services::FirmwareUpgradeCategoryClient& FirmwareUpgrade();
        const services::CanFrameOrigin& FrameOrigin() const;

    private:
        bool SupportsMinor(uint16_t nodeId, uint8_t minimumMinor) const;
        void MarkOnline(uint16_t nodeId);
        void MarkOffline(uint16_t nodeId);
        void RefreshStatus();
        bool HandleCommandAck(uint32_t id, const services::CanFrame& data);
        void ReportIdentity(uint16_t nodeId, const std::optional<FirmwareIdentity>& identity);

        void OnServerOnline(uint16_t nodeId) override;
        void OnServerOffline(uint16_t nodeId) override;
        void OnCommandAckTimeout(uint16_t nodeId, uint8_t category, uint8_t messageType) override;

        void OnSelectControlModeResponse(uint16_t nodeId, can::FocMotorMode activeMode) override;
        void OnCategoryError(uint16_t nodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode) override;
        void OnTelemetryStatus(uint16_t nodeId, can::FocMotorState state, can::FocFaultCode fault) override;
        void OnTelemetryMotion(uint16_t nodeId, foc::RadiansPerSecond speed, foc::Radians position) override;
        void OnTelemetryCurrents(uint16_t nodeId, uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position) override;
        void OnContractVersionResponse(uint16_t nodeId, uint8_t major, uint8_t minor) override;
        void OnAlgorithmsResponse(uint16_t nodeId, const can::FocAlgorithms& algorithms) override;
        void OnParameterResponse(uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value) override;
        void OnFaultRecordResponse(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs) override;
        void OnFaultConditionsResponse(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent) override;

        void OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        can::FocMotorCanClient focClient;
        services::FirmwareUpgradeCategoryClient firmwareUpgrade;
        CommandTracker tracker;
        MotorCommands commands{ focClient.CategoryClient(), tracker };
        NodeData nodeData{ focClient.CategoryClient() };
        std::map<uint16_t, uint8_t> contractMinors;
        std::set<uint16_t> onlineNodes;
        infra::TimerRepeating statusRefresh;
    };
}
