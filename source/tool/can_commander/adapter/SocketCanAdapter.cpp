#ifdef __linux__

#include "source/tool/can_commander/adapter/SocketCanAdapter.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tool
{
    namespace
    {
        // Run a command using fork+execvp without a shell (avoids injection risk).
        // Errors are silently ignored — the caller handles failures at the socket level.
        void ExecCommand(const char* prog, const char* const* args)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                execvp(prog, const_cast<char**>(args));
                _exit(1);
            }
            if (pid > 0)
                waitpid(pid, nullptr, 0);
        }

        // Apply the requested bitrate to a SocketCAN interface by running:
        //   ip link set <iface> down
        //   ip link set <iface> type can bitrate <rate>
        //   ip link set <iface> up
        // This requires CAP_NET_ADMIN (root/sudo).  The function fails silently so
        // that Connect() can still attempt to bind to a pre-configured interface.
        void ApplyBitrate(const char* iface, uint32_t bitrate)
        {
            std::string bitrateStr = std::to_string(bitrate);

            const char* downArgs[] = { "ip", "link", "set", iface, "down", nullptr };
            ExecCommand("ip", downArgs);

            const char* configArgs[] = { "ip", "link", "set", iface, "type", "can",
                "bitrate", bitrateStr.c_str(), nullptr };
            ExecCommand("ip", configArgs);

            const char* upArgs[] = { "ip", "link", "set", iface, "up", nullptr };
            ExecCommand("ip", upArgs);
        }
    }

    SocketCanAdapter::~SocketCanAdapter()
    {
        Disconnect();
    }

    bool SocketCanAdapter::Connect(infra::BoundedConstString interfaceName, uint32_t bitrate)
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

        // Configure bitrate before binding (requires CAP_NET_ADMIN; no-op if lacking privilege)
        ApplyBitrate(ifr.ifr_name, bitrate);

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

    void SocketCanAdapter::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (!IsConnected())
        {
            actionOnCompletion(false);
            return;
        }

        struct can_frame frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.can_id = id.Get29BitId() | CAN_EFF_FLAG;
        frame.can_dlc = static_cast<uint8_t>(std::min(data.size(), static_cast<std::size_t>(8)));
        std::memcpy(frame.data, data.data(), frame.can_dlc);

        auto bytesWritten = write(socketDescriptor, &frame, sizeof(frame));
        bool success = (bytesWritten == sizeof(frame));

        if (success)
        {
            uint32_t rawId = id.Get29BitId();
            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(true, rawId, data);
                });
        }
        else
        {
            NotifyObservers([](auto& observer)
                {
                    observer.OnError("Failed to send CAN frame");
                });
        }

        actionOnCompletion(success);
    }

    void SocketCanAdapter::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
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
            uint32_t rawId = frame.can_id & CAN_EFF_MASK;
            CanFrame data;
            for (uint8_t i = 0; i < frame.can_dlc && i < 8; ++i)
                data.push_back(frame.data[i]);

            NotifyObservers([rawId, &data](auto& observer)
                {
                    observer.OnFrameLog(false, rawId, data);
                });

            if (receiveCallback)
                receiveCallback(hal::Can::Id::Create29BitId(rawId), data);
        }
    }

    void SocketCanAdapter::ValidateDriverAvailability() const
    {
        int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (fd < 0)
            throw std::runtime_error(
                "SocketCAN is not available on this system.\n\n"
                "Make sure the CAN kernel modules are loaded:\n"
                "  sudo modprobe can\n"
                "  sudo modprobe can_raw\n"
                "  sudo modprobe vcan     (for virtual CAN)");
        close(fd);
    }

    std::vector<std::string> SocketCanAdapter::AvailableInterfaces() const
    {
        // ARPHRD_CAN = 280 identifies CAN network interfaces in /sys/class/net/<iface>/type
        static constexpr int arphrdCan = 280;

        std::vector<std::string> result;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net", ec))
        {
            if (ec)
                break;
            std::ifstream typeFile(entry.path() / "type");
            int type = 0;
            if (typeFile >> type && type == arphrdCan)
                result.push_back(entry.path().filename().string());
        }
        return result;
    }
}

#endif
