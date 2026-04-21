#pragma once

#include "hal/interfaces/Can.hpp"
#include "services/network/connection/Connection.hpp"
#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/hardware_bridge/client/can/TcpClientCanbus.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include "tools/hardware_bridge/client/serial/TcpClientSerial.hpp"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace tool
{
    class BridgeController
        : public QObject
    {
        Q_OBJECT

    public:
        explicit BridgeController(QObject* parent = nullptr);
        ~BridgeController() override;

        void Connect(const QString& host, quint16 serialPort, quint16 canPort);
        void Disconnect();
        void SendSerial(const QString& text);
        void SendCan(quint32 id, bool extended, const QByteArray& data);

    signals:
        void SerialConnected();
        void SerialDisconnected();
        void SerialDataReceived(QString text);
        void CanConnected();
        void CanDisconnected();
        void CanFrameReceived(quint32 id, bool extended, QByteArray data);
        void ConnectionError(QString message);

    private:
        class SerialObserver
            : public TcpClientObserver
        {
        public:
            SerialObserver(BridgeController& parent, TcpClient& client);
            void Connected() override;
            void Disconnected() override;

        private:
            BridgeController& parent;
        };

        class CanObserver
            : public TcpClientObserver
        {
        public:
            CanObserver(BridgeController& parent, TcpClient& client);
            void Connected() override;
            void Disconnected() override;

        private:
            BridgeController& parent;
        };

        static services::IPv4Address ParseIPv4(const std::string& host);

        std::thread infraThread;
        std::atomic<bool> stopRequested{ false };

        std::optional<main_::NetworkAdapter> network;
        std::optional<TcpClientSerial> serialClient;
        std::optional<TcpClientCanbus> canClient;
        std::optional<SerialObserver> serialObserver;
        std::optional<CanObserver> canObserver;

        std::mutex sendMutex;
        std::string pendingSerialSend;
        std::optional<std::pair<hal::Can::Id, hal::Can::Message>> pendingCanSend;
    };
}
