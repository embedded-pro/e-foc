#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryClient.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/services/firmware_upgrade/FirmwareVersion.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>

namespace tool
{
    using FocMotorState = can::FocMotorState;
    using FocFaultCode = can::FocFaultCode;

    class CanCommandClient;

    struct FirmwareIdentity
    {
        services::FirmwareVersion running;
        uint8_t boardKind{ 0 };
        services::FirmwareVersion staged;
        bool hasUpgradeService{ true };

        bool operator==(const FirmwareIdentity& other) const = default;
    };

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
        virtual void OnElectricalParametersReceived(uint16_t nodeId, float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs) = 0;
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
        static constexpr infra::Duration commandTimeout{ std::chrono::seconds(2) };
        // The first Start after power-up waits up to 10 s for the rotor to rest before measuring the current offsets.
        static constexpr infra::Duration startCommandTimeout{ std::chrono::seconds(12) };
        static constexpr infra::Duration motionCommandTimeout{ std::chrono::minutes(2) };
        static constexpr infra::Duration readTimeout{ std::chrono::seconds(1) };
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
        static constexpr uint8_t faultHistoryContractMinor{ 17 };

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
        bool SupportsFaultHistory(uint16_t nodeId) const;
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
        bool SendClearFaultHistory(uint16_t nodeId);
        bool SendSetEncoderResolution(uint16_t nodeId, uint32_t countsPerRevolution);
        bool SendSelectAlgorithm(uint16_t nodeId, can::FocControlLoop loop, uint8_t algorithm);
        bool SendSetParameter(uint16_t nodeId, can::FocParameter parameter, float value);

        bool RequestData(uint16_t nodeId);
        void RequestSettings(uint16_t nodeId);
        void RequestReadings(uint16_t nodeId);
        void RequestStatus(uint16_t nodeId);
        void RequestFirmwareIdentity(uint16_t nodeId);
        void RequestFaultHistory(uint16_t nodeId);

        services::FirmwareUpgradeCategoryClient& FirmwareUpgrade();
        const can::CanFrameOrigin& FrameOrigin() const;

    private:
        struct NodeCommand
        {
            bool busy{ false };
            uint8_t pendingCommand{ 0 };
            infra::TimerSingleShot timeout;
        };

        struct Read
        {
            uint8_t commandType{ 0 };
            can::FocParameter parameter{ can::FocParameter::currentBandwidth };
            uint8_t age{ 0 };
        };

        struct NodeReads
        {
            std::deque<Read> queued;
            std::optional<uint8_t> inFlight;
            can::FocParameter inFlightParameter{ can::FocParameter::currentBandwidth };
            infra::TimerSingleShot timeout;
        };

        struct PartialIdentity
        {
            std::array<std::optional<float>, 7> values;
            bool stagingRefused{ false };
        };

        void MarkBusy(uint16_t nodeId, uint8_t commandType);
        void ClearBusy(uint16_t nodeId);
        void CompleteCommand(uint16_t nodeId, uint8_t commandType);
        void MarkOnline(uint16_t nodeId);
        void MarkOffline(uint16_t nodeId);
        void QueueRead(uint16_t nodeId, Read read);
        void SendNextRead(uint16_t nodeId);
        void CompleteRead(uint16_t nodeId, uint8_t commandType);
        bool SendRead(uint16_t nodeId, const Read& read);
        void ForgetReads(uint16_t nodeId);
        void OnAccepted(uint16_t nodeId, uint8_t commandType);
        bool SupportsMinor(uint16_t nodeId, uint8_t minimumMinor) const;
        bool SupportsSetting(uint16_t nodeId, can::FocParameter parameter) const;
        void RefreshStatus();
        static bool IsRead(uint8_t commandType);
        void CollectIdentity(uint16_t nodeId, can::FocParameter parameter, float value);
        void OnReadRefused(uint16_t nodeId, uint8_t commandType);
        void ReportIdentityWhenComplete(uint16_t nodeId);
        bool IsIdentityQueued(uint16_t nodeId) const;

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
        void OnTelemetryCurrents(uint16_t nodeId, uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position) override;
        void OnContractVersionResponse(uint16_t nodeId, uint8_t major, uint8_t minor) override;
        void OnElectricalParamsResponse(uint16_t nodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs) override;
        void OnAlgorithmsResponse(uint16_t nodeId, const can::FocAlgorithms& algorithms) override;
        void OnParameterResponse(uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value) override;
        void OnFaultRecordResponse(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs) override;
        void OnFaultConditionsResponse(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent) override;

        // services::CanBusAdapterObserver
        void OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        bool HandleCommandAck(uint32_t id, const services::CanFrame& data);
        void DecodeTelemetryStatus(uint16_t nodeId, const hal::Can::Message& msg) const;

        can::FocMotorCanClient focClient;
        services::FirmwareUpgradeCategoryClient firmwareUpgrade;
        std::map<uint16_t, NodeCommand> commands;
        std::map<uint16_t, NodeReads> reads;
        std::map<uint16_t, uint8_t> contractMinors;
        std::map<uint16_t, PartialIdentity> identities;
        std::map<uint16_t, can::FocFaultRecord> faultRecords;
        std::set<uint16_t> onlineNodes;
        infra::TimerRepeating statusRefresh;
    };
}
