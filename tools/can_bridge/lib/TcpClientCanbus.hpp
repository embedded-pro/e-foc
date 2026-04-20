#pragma once

#include "hal/interfaces/Can.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/network/connection/Connection.hpp"
#include <array>
#include <cstdint>

namespace tool
{
    class TcpClientCanbus
        : public hal::Can
        , private services::ClientConnectionObserverFactory
    {
    public:
        TcpClientCanbus(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port);

        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;

        static constexpr std::size_t canFrameSize{ 16 };
        static constexpr uint32_t canEffFlag{ 0x80000000 };

        static void EncodeFrame(Id id, const Message& data, std::array<uint8_t, canFrameSize>& buffer);
        static bool DecodeFrame(const uint8_t* raw, Id& outId, Message& outData);

    private:
        services::IPAddress Address() const override;
        uint16_t Port() const override;
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override;
        void ConnectionFailed(ConnectFailReason reason) override;

        class ConnectionHandler
            : public services::ConnectionObserver
        {
        public:
            explicit ConnectionHandler(TcpClientCanbus& parent);

            void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter) override;
            void DataReceived() override;

            void RequestSend(Id id, const Message& data, const infra::Function<void(bool success)>& onDone);

        private:
            TcpClientCanbus& parent;
            std::array<uint8_t, canFrameSize> pendingSendFrame{};
            infra::Function<void(bool success)> pendingSendOnDone;
            std::array<uint8_t, canFrameSize> receiveAccumulator{};
            std::size_t accumulatedBytes{ 0 };
        };

        services::ConnectionFactory& factory;
        services::IPAddress address;
        uint16_t port;

        infra::SharedOptional<ConnectionHandler> connectionHandler;
        infra::Function<void(Id id, const Message& data)> receiveCallback;
        bool connected{ false };
    };
}
