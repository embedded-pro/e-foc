#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/network/connection/Connection.hpp"
#include <array>
#include <cstdint>

namespace tool
{
    class TcpClientSerial
        : public hal::SerialCommunication
        , private services::ClientConnectionObserverFactory
    {
    public:
        TcpClientSerial(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port);

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        services::IPAddress Address() const override;
        uint16_t Port() const override;
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override;
        void ConnectionFailed(ConnectFailReason reason) override;

        class ConnectionHandler
            : public services::ConnectionObserver
        {
        public:
            explicit ConnectionHandler(TcpClientSerial& parent);

            void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter) override;
            void DataReceived() override;

            void RequestSend(infra::ConstByteRange data, infra::Function<void()> onDone);

        private:
            TcpClientSerial& parent;
            infra::ConstByteRange pendingSendData;
            infra::Function<void()> pendingSendOnDone;
        };

        services::ConnectionFactory& factory;
        services::IPAddress address;
        uint16_t port;

        infra::SharedOptional<ConnectionHandler> connectionHandler;
        infra::Function<void(infra::ConstByteRange data)> receiveCallback;

        infra::ConstByteRange queuedSendData;
        infra::Function<void()> queuedSendOnDone;
        bool connected{ false };
    };
}
