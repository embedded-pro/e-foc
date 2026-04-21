#include "tools/hardware_bridge/common/TcpClientBase.hpp"
#include "services/tracer/GlobalTracer.hpp"

namespace tool
{
    TcpClient::TcpClient(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port)
        : factory(factory)
        , address(address)
        , port(port)
    {
        factory.Connect(*this);
    }

    void TcpClient::HandleDisconnected(bool clearConnectionHandler)
    {
        connected = false;
        OnDisconnected(clearConnectionHandler);
        NotifyObservers([](TcpClientObserver& o)
            {
                o.Disconnected();
            });
    }

    void TcpClient::OnConnectionFailed(ConnectFailReason reason)
    {
        HandleDisconnected(true);
    }

    services::IPAddress TcpClient::Address() const
    {
        return address;
    }

    uint16_t TcpClient::Port() const
    {
        return port;
    }

    void TcpClient::ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver)
    {
        connected = true;
        OnConnectionEstablished(std::move(createdObserver));
        NotifyObservers([](TcpClientObserver& o)
            {
                o.Connected();
            });
    }

    void TcpClient::ConnectionFailed(ConnectFailReason reason)
    {
        services::GlobalTracer().Trace() << "TcpClient: connection failed reason=" << static_cast<int>(reason);
        OnConnectionFailed(reason);
    }

}
