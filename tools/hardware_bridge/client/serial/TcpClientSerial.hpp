#pragma once

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/SharedOptional.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include <cstdint>
#include <vector>

namespace tool
{
    class TcpClientSerial
        : public hal::SerialCommunication
        , public TcpClient
    {
    public:
        TcpClientSerial(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port);

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    protected:
        void OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override;
        void OnDisconnected(bool clearConnectionHandler) override;

    private:
        class ConnectionHandler
            : public services::ConnectionObserver
        {
        public:
            explicit ConnectionHandler(TcpClientSerial& parent);

            void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter) override;
            void DataReceived() override;
            void Close() override;
            void Abort() override;

            void RequestSend(infra::ConstByteRange data, infra::Function<void()> onDone);
            void FailPendingSend();

        protected:
            void Detaching() override;

        private:
            void RequestNextSendChunk() const;

            TcpClientSerial& parent;
            std::vector<uint8_t> pendingSendData;
            std::size_t pendingSendOffset{ 0 };
            infra::Function<void()> pendingSendOnDone;
        };

        infra::SharedOptional<ConnectionHandler> connectionHandler;
        infra::SharedPtr<ConnectionHandler> connectionHandlerPtr;
        infra::Function<void(infra::ConstByteRange data)> receiveCallback;
        std::vector<uint8_t> queuedSendData;
        infra::Function<void()> queuedSendOnDone;
    };
}
