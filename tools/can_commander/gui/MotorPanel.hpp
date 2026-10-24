#pragma once

#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "ui/theme/Theme.hpp"
#include <QLabel>
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
        void OnCommandTimeout();
        void OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status);
        void OnCommandRejected(uint8_t commandType, can::FocMotorCategoryError error);
        void OnContractVersion(uint8_t major, uint8_t minor, bool compatible);
        void OnControlModeAcknowledged(can::FocMotorMode activeMode);
        void OnMotorStatus(FocMotorState state, FocFaultCode fault);
        void OnCurrentMeasurement(float idCurrent, float iqCurrent);
        void OnSpeedPosition(float speed, float position);
        void OnBusVoltage(float voltage);
        void OnElectricalParameters(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs);
        void OnMechanicalParameters(float frictionNmSPerRad, float inertiaKgM2);

    signals:
        void StatusChanged();

    private:
        QWidget* CreateHeader();
        void ConnectCommands();
        void ConnectSetpoints();
        void Send(const QString& command, const std::function<bool()>& send);
        void Report(IssueSeverity severity, const QString& message) const;
        void ForgetNodeState();
        QString CommandsUnavailableReason() const;
        void Refresh();
        void RefreshHeader();

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
        std::optional<FocMotorState> lastState;
        std::optional<FocFaultCode> lastFault;
    };
}
