#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include "source/tool/can_commander/gui/CommandPanel.hpp"
#include "source/tool/can_commander/gui/ConnectionPanel.hpp"
#include "source/tool/can_commander/gui/TelemetryPanel.hpp"
#include "source/tool/can_commander/logic/CanCommandClient.hpp"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSocketNotifier>
#include <QSpinBox>
#include <QTimer>

namespace tool
{
    class MainWindow
        : public QMainWindow
        , private CanCommandClientObserver
    {
        Q_OBJECT

    public:
        MainWindow(CanBusAdapter& adapter, QWidget* parent = nullptr);

    private slots:
        void OnConnect();
        void OnDisconnect();

    private:
        void OnCommandTimeout() override;
        void OnBusyChanged(bool busy) override;
        void OnMotorStatusReceived(services::FocMotorState state, services::FocFaultCode fault) override;
        void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) override;
        void OnSpeedPositionReceived(float speed, float position) override;
        void OnBusVoltageReceived(float voltage) override;
        void OnFaultEventReceived(services::FocFaultCode fault) override;
        void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
        void OnConnectionChanged(bool connected) override;
        void OnAdapterError(infra::BoundedConstString message) override;

        void SetupSocketNotifier();
        void TeardownSocketNotifier();

        CanBusAdapter& adapter;
        CanCommandClient client;

        ConnectionPanel* connectionPanel;
        QSpinBox* nodeIdSpin;
        CommandPanel* commandPanel;
        TelemetryPanel* telemetryPanel;
        QPlainTextEdit* logView;

        QTimer timeoutTimer;
        QTimer rxPollTimer;
        QSocketNotifier* socketNotifier = nullptr;
    };
}
