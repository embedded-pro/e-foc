#pragma once

#include "can-lite/drivers/interface/CanBusAdapter.hpp"
#include "tools/can_commander/gui/ConnectionPanel.hpp"
#include "tools/can_commander/gui/EventLogView.hpp"
#include "tools/can_commander/gui/MotorOverview.hpp"
#include "tools/can_commander/gui/MotorPanel.hpp"
#include "tools/can_commander/gui/UpdateBanner.hpp"
#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include "tools/can_commander/support/UpdateChecker.hpp"
#include "tools/can_commander/support/UpdateSession.hpp"
#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSocketNotifier>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <map>

namespace tool
{
    class MainWindow
        : public QMainWindow
        , private CanCommandClientObserver
    {
        Q_OBJECT

    public:
        static constexpr std::size_t maxMotors{ 8 };

        MainWindow(services::CanBusAdapter& adapter, IssueLog& issueLog, QWidget* parent = nullptr);
        ~MainWindow() override;

        void StartAutomaticUpdateChecks();

        void ReportStartupProblem(const QString& title, const QString& message);

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        void OnConnectionChanged(bool connected) override;
        void OnAdapterError(infra::BoundedConstString message) override;
        void OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& frame) override;
        void OnNodeOnlineChanged(uint16_t nodeId, bool online) override;
        void OnBusyChanged(uint16_t nodeId, bool busy) override;
        void OnCommandTimeout(uint16_t nodeId, uint8_t commandType) override;
        void OnCommandAck(uint16_t nodeId, uint8_t categoryId, uint8_t commandType, services::CanAckStatus status) override;
        void OnCommandRejected(uint16_t nodeId, uint8_t commandType, can::FocMotorCategoryError error) override;
        void OnContractVersion(uint16_t nodeId, uint8_t major, uint8_t minor, bool compatible) override;
        void OnControlModeAcknowledged(uint16_t nodeId, can::FocMotorMode activeMode) override;
        void OnMotorStatusReceived(uint16_t nodeId, FocMotorState state, FocFaultCode fault) override;
        void OnCurrentMeasurementReceived(uint16_t nodeId, float idCurrent, float iqCurrent) override;
        void OnSpeedPositionReceived(uint16_t nodeId, float speed, float position) override;
        void OnBusVoltageReceived(uint16_t nodeId, float voltage) override;
        void OnFaultEventReceived(uint16_t nodeId, FocFaultCode fault) override;
        void OnElectricalParametersReceived(uint16_t nodeId, float resistanceOhm, float inductanceMilliHenry, uint8_t polePairs) override;
        void OnMechanicalParametersReceived(uint16_t nodeId, float frictionNmSPerRad, float inertiaKgM2) override;
        void OnAlgorithmsReceived(uint16_t nodeId, const can::FocAlgorithms& algorithms) override;
        void OnParameterReceived(uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value) override;

        void BuildMenus();
        void BuildFileMenu();
        void BuildMotorMenu();
        void BuildHelpMenu();
        void BuildLayout();
        QWidget* BuildSafetyBar();
        QWidget* BuildMotorArea();
        QWidget* BuildEmptyState();
        void ConnectSignals();
        void ConnectUpdateChecker();
        void ConnectUpdateSession();
        void OnUpdateStateChanged(const UpdateState& state);
        void RequestUpdateInstall();
        void RestartForUpdate();
        void RelaunchUpdatedVersion();
        void RestoreSettings();
        void SaveSettings() const;

        void Connect();
        void Disconnect();
        void PromptAddMotor();
        MotorPanel* AddMotor(uint16_t nodeId);
        void CloseMotor(int tabIndex);
        void SelectMotor(uint16_t nodeId);
        MotorPanel* Motor(uint16_t nodeId) const;
        void RefreshMotor(MotorPanel& motor);
        void EmergencyStopAll();
        void Report(IssueSeverity severity, const QString& message);
        void UpdateSafetyControls();
        void UpdateStatusBar();

        void CheckForUpdates(bool userInitiated);
        void ReportIssue();
        void OpenLogFolder();
        void ShowAbout();

        void SetupSocketNotifier();
        void TeardownSocketNotifier();

        services::CanBusAdapter& adapter;
        IssueLog& issueLog;
        CanCommandClient client;
        UpdateChecker updateChecker;
        UpdateSession updateSession;

        UpdateBanner* updateBanner{ nullptr };
        ConnectionPanel* connectionPanel{ nullptr };
        QPushButton* emergencyStopButton{ nullptr };
        MotorOverview* overview{ nullptr };
        QStackedWidget* motorArea{ nullptr };
        QTabWidget* motorTabs{ nullptr };
        QPushButton* addMotorButton{ nullptr };
        EventLogView* eventLog{ nullptr };
        QSplitter* logSplitter{ nullptr };

        QLabel* motorsStatus{ nullptr };
        QAction* addMotorAction{ nullptr };
        QAction* emergencyStopAction{ nullptr };
        QAction* showFramesAction{ nullptr };
        QAction* automaticUpdatesAction{ nullptr };

        QTimer rxPollTimer;
        QTimer automaticUpdateTimer;
        QSocketNotifier* socketNotifier{ nullptr };

        std::map<uint16_t, MotorPanel*> motors;
        bool busConnected{ false };
        bool userRequestedUpdateCheck{ false };
        UpdatePhase updatePhase{ UpdatePhase::idle };
    };
}
