#pragma once

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
        bool Send(uint32_t id, const CanFrame& data) override;

        int FileDescriptor() const override;
        void ProcessReadEvent() override;

    private:
        int socketDescriptor = -1;
    };
}

#endif
