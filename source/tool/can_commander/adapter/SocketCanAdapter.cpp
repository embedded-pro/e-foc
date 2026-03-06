#ifdef __linux__

#include "source/tool/can_commander/adapter/SocketCanAdapter.hpp"
#include <algorithm>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tool
{
    SocketCanAdapter::~SocketCanAdapter()
    {
        Disconnect();
    }

    bool SocketCanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t /*bitrate*/)
    {
        if (IsConnected())
            Disconnect();

        socketDescriptor = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (socketDescriptor < 0)
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to create CAN socket");
                });
            return false;
        }

        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));

        auto nameLength = std::min(interfaceName.size(), static_cast<std::size_t>(IFNAMSIZ - 1));
        std::memcpy(ifr.ifr_name, interfaceName.data(), nameLength);
        ifr.ifr_name[nameLength] = '\0';

        if (ioctl(socketDescriptor, SIOCGIFINDEX, &ifr) < 0)
        {
            close(socketDescriptor);
            socketDescriptor = -1;
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("CAN interface not found");
                });
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
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to bind CAN socket");
                });
            return false;
        }

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(true);
            });
        return true;
    }

    void SocketCanAdapter::Disconnect()
    {
        if (socketDescriptor >= 0)
        {
            close(socketDescriptor);
            socketDescriptor = -1;
        }

        NotifyObservers([](auto& observer)
            {
                observer.OnConnectionChanged(false);
            });
    }

    bool SocketCanAdapter::IsConnected() const
    {
        return socketDescriptor >= 0;
    }

    bool SocketCanAdapter::Send(uint32_t id, const CanFrame& data)
    {
        if (!IsConnected())
            return false;

        struct can_frame frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.can_id = id | CAN_EFF_FLAG;
        frame.can_dlc = static_cast<uint8_t>(std::min(data.size(), static_cast<std::size_t>(8)));
        std::memcpy(frame.data, data.data(), frame.can_dlc);

        auto bytesWritten = write(socketDescriptor, &frame, sizeof(frame));
        if (bytesWritten != sizeof(frame))
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to send CAN frame");
                });
            return false;
        }

        return true;
    }

    int SocketCanAdapter::FileDescriptor() const
    {
        return socketDescriptor;
    }

    void SocketCanAdapter::ProcessReadEvent()
    {
        struct can_frame frame;
        auto bytesRead = read(socketDescriptor, &frame, sizeof(frame));

        if (bytesRead == sizeof(frame))
        {
            uint32_t id = frame.can_id & CAN_EFF_MASK;
            CanFrame data;
            for (uint8_t i = 0; i < frame.can_dlc && i < 8; ++i)
                data.push_back(frame.data[i]);

            NotifyObservers([id, &data](auto& observer)
                {
                    observer.OnFrameReceived(id, data);
                });
        }
    }
}

#endif
