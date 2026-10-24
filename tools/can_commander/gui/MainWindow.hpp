#pragma once

#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/gui/ConnectionPanel.hpp"
#include "tools/can_commander/gui/EventLogView.hpp"
#include "tools/can_commander/gui/TelemetryPanel.hpp"
#include "tools/can_commander/gui/UpdateBanner.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "tools/can_commander/support/UpdateChecker.hpp"
#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSocketNotifier>
#include <QSplitter>
#include <QTimer>
#include <functional>
#include <optional>

namespace tool
{
    class MainWindow
        : public QMainWindow
        , private CanCommandClientObserver
    {
        Q_OBJECT

    public:
        MainWindow(CanBusAdapter& adapter, IssueLog& issueLog, QWidget* parent = nullptr);
        ~MainWindow() override;

        // Honours the user's "Check for Updates at Startup" choice; never blocks.
        void CheckForUpdatesOnStartup();

        // Logs a problem found while starting and tells the user, without closing the window.
        void ReportStartupProblem(const QString& title, const QString& message);

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        enum class Contract
        {
            unknown,
            compatible,
            incompatible
        };

        void OnCommandTimeout() override;
        void OnBusyChanged(bool busy) override;
        void OnMotorStatusReceived(FocMotorState state, FocFaultCode fault) override;
        void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) override;
        void OnSpeedPositionReceived(float speed, float position) override;
        void OnBusVoltageReceived(float voltage) override;
        void OnFaultEventReceived(FocFaultCode fault) override;
        void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
        void OnConnectionChanged(bool connected) override;
        void OnNodeOnlineChanged(bool online) override;
        void OnAdapterError(infra::BoundedConstString message) override;
        void OnControlModeAcknowledged(can::FocMotorMode activeMode) override;
        void OnCommandAck(uint8_t categoryId, uint8_t commandType, services::CanAckStatus status) override;
        void OnContractVersion(uint8_t major, uint8_t minor, bool compatible) override;
        void OnElectricalParametersReceived(float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs) override;
        void OnMechanicalParametersReceived(float frictionNmSPerRad, float inertiaKgM2) override;

        void BuildMenus();
        void BuildFileMenu();
        void BuildHelpMenu();
        void BuildLayout();
        QWidget* BuildSafetyBar();
        void BuildStatusBar();
        void ConnectCommands();
        void ConnectSetpoints();
        void ConnectUpdateChecker();
        void RestoreSettings();
        void SaveSettings() const;

        void Connect();
        void Disconnect();
        void SelectNode(uint16_t nodeId);
        void ForgetNodeState();
        void EmergencyStop();
        void Send(const QString& command, const std::function<bool()>& send);
        void Report(IssueSeverity severity, const QString& message);

        QString CommandsUnavailableReason() const;
        void UpdateCommandsEnabled();
        void UpdateStatusBar();

        void CheckForUpdates(bool userInitiated);
        void ReportIssue();
        void OpenLogFolder();
        void ShowAbout();

        void SetupSocketNotifier();
        void TeardownSocketNotifier();

        CanBusAdapter& adapter;
        IssueLog& issueLog;
        CanCommandClient client;
        UpdateChecker updateChecker;

        UpdateBanner* updateBanner{ nullptr };
        ConnectionPanel* connectionPanel{ nullptr };
        QPushButton* emergencyStopButton{ nullptr };
        CommandPanel* commandPanel{ nullptr };
        TelemetryPanel* telemetryPanel{ nullptr };
        EventLogView* eventLog{ nullptr };
        QSplitter* workSplitter{ nullptr };
        QSplitter* logSplitter{ nullptr };

        QLabel* nodeStatus{ nullptr };
        QLabel* firmwareStatus{ nullptr };
        QLabel* activityStatus{ nullptr };
        QAction* showFramesAction{ nullptr };
        QAction* checkAtStartupAction{ nullptr };

        QTimer timeoutTimer;
        QTimer rxPollTimer;
        QSocketNotifier* socketNotifier{ nullptr };

        bool busConnected{ false };
        std::optional<bool> nodeOnline;
        Contract contract{ Contract::unknown };
        QString contractVersion;
        QString pendingCommand;
        std::optional<FocMotorState> lastState;
        std::optional<FocFaultCode> lastFault;
        bool userRequestedUpdateCheck{ false };
    };
}
