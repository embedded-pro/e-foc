#pragma once

#include "can-lite/core/MonitoredCan.hpp"
#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "can-lite/drivers/implementation/VirtualBusPort.hpp"
#include <cstdint>

namespace virtual_motor
{
    class VirtualCanPort
        : public services::MonitoredCan
    {
    public:
        explicit VirtualCanPort(uint16_t hubPort = services::loopback::virtualCanHubPort);

        bool Connect(infra::BoundedConstString bus, uint32_t bitrate);
        bool IsConnected() const;
        void Poll();
        void Mute(bool muted);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
        void SetOnError(const infra::Function<void(services::CanBusError)>& handler) override;

    private:
        class ErrorObserver
            : public services::CanBusAdapterObserver
        {
        public:
            ErrorObserver(services::CanBusAdapter& adapter, VirtualCanPort& owner);

            void OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data) override;
            void OnError(infra::BoundedConstString message) override;
            void OnConnectionChanged(bool connected) override;

        private:
            VirtualCanPort& owner;
        };

        void ReportError();

        services::VirtualBusPort port;
        services::SlcanAdapter adapter{ port };
        ErrorObserver errorObserver{ adapter, *this };
        infra::Function<void(services::CanBusError)> onError;
        infra::Function<void(Id id, const Message& data)> onReceived;
        infra::Function<void(bool success)> mutedSendDone;
        bool muted{ false };
    };
}
