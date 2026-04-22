#pragma once

#include "infra/util/Observer.hpp"
#include "services/network/connection/Connection.hpp"
#include <cstdint>

namespace tool
{
    class TcpClient;

    class TcpClientObserver
        : public infra::Observer<TcpClientObserver, TcpClient>
    {
    public:
        using infra::Observer<TcpClientObserver, TcpClient>::Observer;

        virtual void Connected() = 0;
        virtual void Disconnected() = 0;
    };

    class TcpClient
        : public infra::Subject<TcpClientObserver>
        , private services::ClientConnectionObserverFactory
    {
    public:
        TcpClient(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port);
        virtual ~TcpClient() = default;

        bool IsConnected() const;

    protected:
        void HandleDisconnected(bool clearConnectionHandler);

        virtual void OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) = 0;
        virtual void OnDisconnected(bool clearConnectionHandler) = 0;
        virtual void OnConnectionFailed(ConnectFailReason reason);

    private:
        services::IPAddress Address() const override;
        uint16_t Port() const override;
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override;
        void ConnectionFailed(ConnectFailReason reason) override;

        bool connected{ false };
        services::IPAddress address;
        uint16_t port;
    };
}
