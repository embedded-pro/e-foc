#include "tools/hardware_bridge/client/gui/BridgeController.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#include <QMetaObject>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <netdb.h>

namespace
{
    std::string ResolveToIPv4(const std::string& host)
    {
        unsigned int o0 = 0, o1 = 0, o2 = 0, o3 = 0;
        int consumed = 0;
        if (std::sscanf(host.c_str(), "%u.%u.%u.%u%n", &o0, &o1, &o2, &o3, &consumed) == 4 && consumed == static_cast<int>(host.size()))
            return host;

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0 && result != nullptr)
        {
            char buf[INET_ADDRSTRLEN];
            const auto* sa = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
            if (inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)) != nullptr)
            {
                freeaddrinfo(result);
                return buf;
            }
            freeaddrinfo(result);
        }

        throw tool::BridgeArgumentException("cannot resolve host: " + host);
    }
}

namespace tool
{
    // --- SerialObserver ---

    BridgeController::SerialObserver::SerialObserver(BridgeController& parent, TcpClient& client)
        : TcpClientObserver(client)
        , parent(parent)
    {}

    void BridgeController::SerialObserver::Connected()
    {
        QMetaObject::invokeMethod(&parent, [this]()
            {
                emit parent.SerialConnected();
            },
            Qt::QueuedConnection);
    }

    void BridgeController::SerialObserver::Disconnected()
    {
        QMetaObject::invokeMethod(&parent, [this]()
            {
                emit parent.SerialDisconnected();
            },
            Qt::QueuedConnection);
    }

    // --- CanObserver ---

    BridgeController::CanObserver::CanObserver(BridgeController& parent, TcpClient& client)
        : TcpClientObserver(client)
        , parent(parent)
    {}

    void BridgeController::CanObserver::Connected()
    {
        QMetaObject::invokeMethod(&parent, [this]()
            {
                emit parent.CanConnected();
            },
            Qt::QueuedConnection);
    }

    void BridgeController::CanObserver::Disconnected()
    {
        QMetaObject::invokeMethod(&parent, [this]()
            {
                emit parent.CanDisconnected();
            },
            Qt::QueuedConnection);
    }

    // --- BridgeController ---

    BridgeController::BridgeController(QObject* parent)
        : QObject(parent)
    {}

    BridgeController::~BridgeController()
    {
        Disconnect();
    }

    void BridgeController::Connect(const QString& host, quint16 serialPort, quint16 canPort)
    {
        if (infraThread.joinable())
            return;

        stopRequested = false;
        const std::string hostStr = host.toStdString();

        infraThread = std::thread([this, hostStr, serialPort, canPort]()
            {
                try
                {
                    const services::IPv4Address address = tool::ParseIPv4(ResolveToIPv4(hostStr));

                    network.emplace();

                    serialClient.emplace(network->ConnectionFactory(), address, serialPort);
                    serialObserver.emplace(*this, *serialClient);
                    serialClient->ReceiveData([this](infra::ConstByteRange data)
                        {
                            const QByteArray bytes(
                                reinterpret_cast<const char*>(data.begin()),
                                static_cast<int>(data.size()));
                            QMetaObject::invokeMethod(this, [this, bytes]()
                                {
                                    emit SerialDataReceived(bytes);
                                },
                                Qt::QueuedConnection);
                        });

                    canClient.emplace(network->ConnectionFactory(), address, canPort);
                    canObserver.emplace(*this, *canClient);
                    canClient->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& msg)
                        {
                            const bool extended = id.Is29BitId();
                            const quint32 rawId = extended ? id.Get29BitId() : id.Get11BitId();
                            const QByteArray data(reinterpret_cast<const char*>(msg.data()),
                                static_cast<int>(msg.size()));
                            QMetaObject::invokeMethod(this, [this, rawId, extended, data]()
                                {
                                    emit CanFrameReceived(rawId, extended, data);
                                },
                                Qt::QueuedConnection);
                        });

                    network->ExecuteUntil([this]()
                        {
                            return stopRequested.load();
                        });
                }
                catch (const BridgeArgumentException& e)
                {
                    const QString msg = QString::fromUtf8(e.what());
                    QMetaObject::invokeMethod(this, [this, msg]()
                        {
                            emit ConnectionError(msg);
                        },
                        Qt::QueuedConnection);
                }
                catch (const std::exception& e)
                {
                    const QString msg = QString::fromUtf8(e.what());
                    QMetaObject::invokeMethod(this, [this, msg]()
                        {
                            emit ConnectionError(msg);
                        },
                        Qt::QueuedConnection);
                }

                canObserver.reset();
                serialObserver.reset();
                serialClient.reset();
                canClient.reset();
                network.reset();
            });
    }

    void BridgeController::Disconnect()
    {
        stopRequested = true;
        if (infraThread.joinable())
            infraThread.join();
        stopRequested = false;
    }

    void BridgeController::SendSerial(const QByteArray& data)
    {
        if (!serialClient)
            return;

        {
            std::lock_guard<std::mutex> lock(sendMutex);
            pendingSerialSend.assign(data.constData(), data.constData() + data.size());
        }

        infra::EventDispatcher::Instance().Schedule([this]()
            {
                std::string toSend;
                {
                    std::lock_guard<std::mutex> lock(sendMutex);
                    toSend = std::move(pendingSerialSend);
                }
                if (serialClient && !toSend.empty())
                {
                    const infra::ConstByteRange range(
                        reinterpret_cast<const uint8_t*>(toSend.data()),
                        reinterpret_cast<const uint8_t*>(toSend.data()) + toSend.size());
                    serialClient->SendData(range, []() {});
                }
            });
    }

    void BridgeController::SendCan(quint32 id, bool extended, const QByteArray& data)
    {
        if (!canClient)
            return;

        hal::Can::Id canId = extended
                                 ? hal::Can::Id::Create29BitId(id)
                                 : hal::Can::Id::Create11BitId(id);

        hal::Can::Message msg;
        const int len = std::min(static_cast<int>(data.size()), 8);
        for (int i = 0; i < len; ++i)
            msg.push_back(static_cast<uint8_t>(data[i]));

        {
            std::lock_guard<std::mutex> lock(sendMutex);
            pendingCanSend = std::make_pair(canId, msg);
        }

        infra::EventDispatcher::Instance().Schedule([this]()
            {
                std::optional<std::pair<hal::Can::Id, hal::Can::Message>> send;
                {
                    std::lock_guard<std::mutex> lock(sendMutex);
                    send = std::move(pendingCanSend);
                }
                if (canClient && send)
                    canClient->SendData(send->first, send->second, [](bool) {});
            });
    }
}
