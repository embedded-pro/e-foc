#pragma once

#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

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

        void RestoreSettings(const QSettings& settings);
        void SaveSettings(QSettings& settings) const;

    signals:
        void ConnectRequested();
        void DisconnectRequested();

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
        QPushButton* connectButton;
        QPushButton* disconnectButton;
        QLabel* statusLabel;
        bool connected{ false };
    };
}
