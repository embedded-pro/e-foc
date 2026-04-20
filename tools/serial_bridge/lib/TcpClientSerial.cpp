#include "tools/serial_bridge/lib/TcpClientSerial.hpp"
#include "infra/stream/InputStream.hpp"
#include "infra/stream/OutputStream.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace tool
{
    TcpClientSerial::TcpClientSerial(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port)
        : factory(factory)
        , address(address)
        , port(port)
    {
        factory.Connect(*this);
    }

    void TcpClientSerial::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        if (connected && connectionHandler)
            connectionHandler->RequestSend(data, actionOnCompletion);
        else
        {
            queuedSendData = data;
            queuedSendOnDone = actionOnCompletion;
        }
    }

    void TcpClientSerial::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        receiveCallback = dataReceived;
    }

    services::IPAddress TcpClientSerial::Address() const
    {
        return address;
    }

    uint16_t TcpClientSerial::Port() const
    {
        return port;
    }

    void TcpClientSerial::ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver)
    {
        connected = true;
        createdObserver(connectionHandler.Emplace(*this));

        if (!queuedSendData.empty() && connectionHandler)
        {
            connectionHandler->RequestSend(queuedSendData, queuedSendOnDone);
            queuedSendData = {};
            queuedSendOnDone = nullptr;
        }
    }

    void TcpClientSerial::ConnectionFailed(ConnectFailReason reason)
    {
        std::fprintf(stderr, "TcpClientSerial: connection failed (reason=%d)\n", static_cast<int>(reason));
        std::abort();
    }

    TcpClientSerial::ConnectionHandler::ConnectionHandler(TcpClientSerial& parent)
        : parent(parent)
    {}

    void TcpClientSerial::ConnectionHandler::SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter)
    {
        if (!pendingSendData.empty())
        {
            infra::DataOutputStream::WithErrorPolicy stream(*streamWriter);
            stream << pendingSendData;
            pendingSendData = {};
            streamWriter = nullptr;

            if (pendingSendOnDone)
            {
                auto onDone = pendingSendOnDone;
                pendingSendOnDone = nullptr;
                onDone();
            }
        }
    }

    void TcpClientSerial::ConnectionHandler::DataReceived()
    {
        if (!parent.receiveCallback)
            return;

        auto stream = services::ConnectionObserver::Subject().ReceiveStream();
        infra::DataInputStream::WithErrorPolicy dataStream(*stream, infra::noFail);

        while (!dataStream.Empty())
        {
            std::array<uint8_t, 256> buffer;
            auto bytesToRead = std::min(buffer.size(), static_cast<std::size_t>(dataStream.Available()));
            infra::ByteRange range(buffer.data(), buffer.data() + bytesToRead);
            dataStream >> range;
            parent.receiveCallback(infra::ConstByteRange(buffer.data(), buffer.data() + bytesToRead));
        }

        services::ConnectionObserver::Subject().AckReceived();
    }

    void TcpClientSerial::ConnectionHandler::RequestSend(infra::ConstByteRange data, infra::Function<void()> onDone)
    {
        pendingSendData = data;
        pendingSendOnDone = onDone;

        auto sendSize = std::min(data.size(), services::ConnectionObserver::Subject().MaxSendStreamSize());
        services::ConnectionObserver::Subject().RequestSendStream(sendSize);
    }
}
