#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"
#include "source/tool/can_commander/gui/CommandPanel.hpp"
#include "source/tool/can_commander/gui/ConnectionPanel.hpp"
#include "source/tool/can_commander/gui/TelemetryPanel.hpp"
#include "source/tool/can_commander/logic/CanCommandClient.hpp"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace tool
{
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(CanBusAdapter& adapter, QWidget* parent = nullptr);

    private slots:
        void OnConnect();
        void OnDisconnect();
        void OnBusyChanged(bool busy);
        void OnConnectionChanged(bool connected);
        void OnCommandTimeout();
        void OnError(QString message);
        void OnFrameLog(QString direction, uint32_t id, QByteArray data);

    private:
        CanBusAdapter& adapter;
        CanCommandClient client;

        ConnectionPanel* connectionPanel;
        QSpinBox* nodeIdSpin;
        CommandPanel* commandPanel;
        TelemetryPanel* telemetryPanel;
        QPlainTextEdit* logView;

        void ConnectSignals();
    };
}
