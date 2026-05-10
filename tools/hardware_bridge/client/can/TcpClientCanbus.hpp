#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/SharedOptional.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include <array>
#include <cstdint>

namespace tool
{
    class TcpClientCanbus
        : public hal::Can
        , public TcpClient
    {
    public:
        TcpClientCanbus(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port);
        ~TcpClientCanbus() override;

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

    protected:
        void OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override;
        void OnDisconnected(bool clearConnectionHandler) override;

    private:
        class ConnectionHandler
            : public services::ConnectionObserver
        {
        public:
            explicit ConnectionHandler(TcpClientCanbus& parent);

            void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter) override;
            void DataReceived() override;
            void Close() override;
            void Abort() override;

            void RequestSend(Id id, const Message& data, const infra::Function<void(bool success)>& onDone);
            void FailPendingSend(bool success);

        protected:
            void Detaching() override;

        private:
            static constexpr std::size_t canFrameSize{ 16 };
            TcpClientCanbus& parent;
            std::array<uint8_t, canFrameSize> pendingSendFrame{};
            infra::Function<void(bool success)> pendingSendOnDone;
            std::array<uint8_t, canFrameSize> receiveAccumulator{};
            std::size_t accumulatedBytes{ 0 };
        };

        infra::SharedOptional<ConnectionHandler> connectionHandler;
        infra::SharedPtr<ConnectionHandler> connectionHandlerPtr;
        infra::Function<void(Id id, const Message& data)> receiveCallback;
    };

}
