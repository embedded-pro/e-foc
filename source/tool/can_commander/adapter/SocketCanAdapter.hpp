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

        bool Connect(infra::BoundedConstString interfaceName, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        int FileDescriptor() const override;
        void ProcessReadEvent() override;

        std::vector<std::string> AvailableInterfaces() const override;
        void ValidateDriverAvailability() const override;

    private:
        int socketDescriptor = -1;
        infra::Function<void(Id, const Message&)> receiveCallback;
    };
}

#endif
