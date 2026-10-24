#pragma once

#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

namespace tool
{
    class ConnectionPanel
        : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit ConnectionPanel(CanBusAdapter& adapter, QWidget* parent = nullptr);

        QString InterfaceName() const;
        uint32_t Bitrate() const;
        QString BitrateText() const;
        uint16_t NodeId() const;

        void RestoreSettings(const QSettings& settings);
        void SaveSettings(QSettings& settings) const;

    signals:
        void ConnectRequested();
        void DisconnectRequested();
        void NodeIdChanged(uint16_t nodeId);

    public slots:
        void OnConnectionChanged(bool connected);

    private slots:
        void RefreshInterfaces();

    private:
        static void AddLabelled(QHBoxLayout& layout, const char* text, QWidget* field);
        void CreateFields(QHBoxLayout& layout);
        void CreateButtons(QHBoxLayout& layout);
        void ConnectSignals();
        void UpdateConnectEnabled();

        CanBusAdapter& adapter;
        QComboBox* interfaceCombo;
        QPushButton* refreshButton;
        QComboBox* bitrateCombo;
        QSpinBox* nodeIdSpin;
        QPushButton* connectButton;
        QPushButton* disconnectButton;
        QLabel* statusLabel;
        bool connected{ false };
    };
}
