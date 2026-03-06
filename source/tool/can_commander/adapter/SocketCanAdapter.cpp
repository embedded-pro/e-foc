#ifdef __linux__

#include "source/tool/can_commander/adapter/SocketCanAdapter.hpp"
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tool
{
    SocketCanAdapter::SocketCanAdapter(QObject* parent)
        : CanBusAdapter(parent)
    {}

    SocketCanAdapter::~SocketCanAdapter()
    {
        Disconnect();
    }

    bool SocketCanAdapter::Connect(const QString& interface, uint32_t /*bitrate*/)
    {
        if (IsConnected())
            Disconnect();

        socketDescriptor = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (socketDescriptor < 0)
        {
            emit ErrorOccurred("Failed to create CAN socket");
            return false;
        }

        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, interface.toStdString().c_str(), IFNAMSIZ - 1);

        if (ioctl(socketDescriptor, SIOCGIFINDEX, &ifr) < 0)
        {
            close(socketDescriptor);
            socketDescriptor = -1;
            emit ErrorOccurred("CAN interface '" + interface + "' not found");
            return false;
        }

        struct sockaddr_can addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(socketDescriptor, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            close(socketDescriptor);
            socketDescriptor = -1;
            emit ErrorOccurred("Failed to bind CAN socket to '" + interface + "'");
            return false;
        }

        readNotifier = new QSocketNotifier(socketDescriptor, QSocketNotifier::Read, this);
        connect(readNotifier, &QSocketNotifier::activated, this, &SocketCanAdapter::OnReadyRead);

        emit ConnectionChanged(true);
        return true;
    }

    void SocketCanAdapter::Disconnect()
    {
        if (readNotifier != nullptr)
        {
            readNotifier->setEnabled(false);
            delete readNotifier;
            readNotifier = nullptr;
        }

        if (socketDescriptor >= 0)
        {
            close(socketDescriptor);
            socketDescriptor = -1;
        }

        emit ConnectionChanged(false);
    }

    bool SocketCanAdapter::IsConnected() const
    {
        return socketDescriptor >= 0;
    }

    bool SocketCanAdapter::Send(uint32_t id, const QByteArray& data)
    {
        if (!IsConnected())
            return false;

        struct can_frame frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.can_id = id | CAN_EFF_FLAG;
        frame.can_dlc = static_cast<uint8_t>(std::min(static_cast<int>(data.size()), 8));
        std::memcpy(frame.data, data.constData(), frame.can_dlc);

        auto bytesWritten = write(socketDescriptor, &frame, sizeof(frame));
        if (bytesWritten != sizeof(frame))
        {
            emit ErrorOccurred("Failed to send CAN frame");
            return false;
        }

        return true;
    }

    void SocketCanAdapter::OnReadyRead()
    {
        struct can_frame frame;
        auto bytesRead = read(socketDescriptor, &frame, sizeof(frame));

        if (bytesRead == sizeof(frame))
        {
            uint32_t id = frame.can_id & CAN_EFF_MASK;
            QByteArray data(reinterpret_cast<const char*>(frame.data), frame.can_dlc);
            emit FrameReceived(id, data);
        }
    }
}

#endif
