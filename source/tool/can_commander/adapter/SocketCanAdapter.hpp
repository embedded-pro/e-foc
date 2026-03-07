#pragma once

// SocketCanAdapter uses the Linux SocketCAN kernel subsystem and is only
// available on Linux. For Windows support a separate adapter using e.g.
// PCAN (PCANBasic API) or Vector XL would be needed.
#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"

#ifdef __linux__

namespace tool
{
    class SocketCanAdapter : public CanBusAdapter
    {
    public:
        SocketCanAdapter() = default;
        ~SocketCanAdapter() override;

        // Connect brings the interface down, applies the requested bitrate, then
        // brings it back up before binding the raw CAN socket.  Bitrate
        // configuration is performed by running "ip link set" and therefore
        // requires root/CAP_NET_ADMIN; if the process lacks the privilege the
        // commands fail silently and the interface must already be configured.
        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;
        bool Send(uint32_t id, const CanFrame& data) override;

        int FileDescriptor() const override;
        void ProcessReadEvent() override;

        // Scans /sys/class/net/ and returns names of all CAN interfaces present.
        std::vector<std::string> AvailableInterfaces() const override;

        // Throws std::runtime_error if the SocketCAN kernel subsystem is not
        // available (i.e. the 'can' module is not loaded).
        void ValidateDriverAvailability() const override;

    private:
        int socketDescriptor = -1;
    };
}

#endif
