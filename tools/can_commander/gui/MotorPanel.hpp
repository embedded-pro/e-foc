#pragma once

#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include "tools/can_commander/logic/RecoveryAdvice.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "ui/theme/Theme.hpp"
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QWidget>
#include <functional>
#include <optional>

namespace tool
{
    class MotorPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        using Reporter = std::function<void(IssueSeverity, const QString&)>;

        MotorPanel(CanCommandClient& client, uint16_t nodeId, Reporter reporter, QWidget* parent = nullptr);

        uint16_t NodeId() const;
        QString Title() const;
        QString Summary() const;
        ui::theme::StatusLevel Level() const;
        bool AcceptsEmergencyStop() const;

        bool SendEmergencyStop();

        void SetBusConnected(bool connected);
        void OnNodeOnlineChanged(bool online);
        void OnBusyChanged(bool busy);
        void OnCommandTimeout(uint8_t commandType);
        void OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status);
        void OnCommandRejected(uint8_t commandType, can::FocMotorCategoryError error);
        void OnContractVersion(uint8_t major, uint8_t minor, bool compatible);
        void OnControlModeAcknowledged(can::FocMotorMode activeMode);
        void OnMotorStatus(FocMotorState state, FocFaultCode fault);
        void OnCurrentStream(float idCurrent, float iqCurrent, float position);
        void OnSpeedPosition(float speed, float position);
        void OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs);
        void OnAlgorithms(const can::FocAlgorithms& algorithms);
        void OnParameter(can::FocParameter parameter, can::FocParameterValue value);

    signals:
        void StatusChanged();
        void ShowRequested();

    private:
        QWidget* CreateHeader();
        void ConnectCommands();
        void ConnectSetpoints();
        void SendStopMotor();
        void SendClearFault();
        void SendAlign();
        void SendSetpoint(can::FocMotorMode mode, int32_t counts);
        bool SendSetpointValue(can::FocMotorMode mode, float value);
        void OfferRecovery(uint8_t commandType, Refusal refusal);
        void ShowRecovery(uint8_t commandType, const RecoveryAdvice& advice);
        void RunRecovery(RecoveryAction action);
        void CloseRecovery();
        void ConnectCommissioning();
        void ConnectDiagnostics();
        void ConfirmClearCalibration();
        void ConnectSettings();
        bool Send(const QString& command, uint8_t commandType, const std::function<bool()>& send);
        void Report(IssueSeverity severity, const QString& message) const;
        void ForgetNodeState();
        QString CommandsUnavailableReason() const;
        QString SettingsUnavailableReason() const;
        QString DiagnosticsUnavailableReason() const;
        void Refresh();
        void RefreshHeader();

        void Apply(SettingsPage& page);
        void SendCurrentChange();
        bool SendChange(const SettingChange& change);
        QString Describe(const SettingChange& change) const;
        bool IsChangeAnswer(uint8_t commandType) const;
        void OnChangeAccepted();
        void StopApplying(const QString& reason, bool currentSent = true);
        void ForgetControlMode();
        bool IsLegacy() const;

        CanCommandClient& client;
        const uint16_t nodeId;
        Reporter reporter;

        QLabel* onlineLabel{ nullptr };
        QLabel* firmwareLabel{ nullptr };
        QLabel* activityLabel{ nullptr };
        CommandPanel* commandPanel{ nullptr };
        TelemetryPanel* telemetryPanel{ nullptr };

        bool busConnected{ false };
        std::optional<bool> nodeOnline;
        QString contractVersion;
        QString pendingCommand;
        uint8_t pendingCommandType{ 0 };
        std::optional<FocMotorState> lastState;
        std::optional<FocFaultCode> lastFault;

        struct PendingSetpoint
        {
            can::FocMotorMode mode;
            int32_t counts;
        };

        std::optional<PendingSetpoint> pendingSetpoint;
        QPointer<QMessageBox> recoveryBox;
        RecoveryAdvice recoveryAdvice;

        ChangeSequence changes;
        SettingsPage* applyingPage{ nullptr };
    };
}
