#include "tools/can_bridge/lib/TcpClientCanbus.hpp"
#include "infra/stream/InputStream.hpp"
#include "infra/stream/OutputStream.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <algorithm>
#include <cstring>

namespace tool
{
    TcpClientCanbus::TcpClientCanbus(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port)
        : factory(factory)
        , address(address)
        , port(port)
    {
        factory.Connect(*this);
    }

    void TcpClientCanbus::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (connected && connectionHandler)
            connectionHandler->RequestSend(id, data, actionOnCompletion);
        else if (actionOnCompletion)
            actionOnCompletion(false);
    }

    void TcpClientCanbus::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    services::IPAddress TcpClientCanbus::Address() const
    {
        return address;
    }

    uint16_t TcpClientCanbus::Port() const
    {
        return port;
    }

    void TcpClientCanbus::ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver)
    {
        connected = true;
        createdObserver(connectionHandler.Emplace(*this));
    }

    void TcpClientCanbus::ConnectionFailed(ConnectFailReason reason)
    {
        services::GlobalTracer().Trace() << "TcpClientCanbus: connection failed reason=" << static_cast<int>(reason);
        HandleDisconnected(true);
    }

    TcpClientCanbus::ConnectionHandler::ConnectionHandler(TcpClientCanbus& parent)
        : parent(parent)
    {}

    void TcpClientCanbus::ConnectionHandler::SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter)
    {
        if (!pendingSendOnDone)
        {
            streamWriter = nullptr;
            return;
        }

        infra::DataOutputStream::WithErrorPolicy stream(*streamWriter);
        stream << infra::ConstByteRange(pendingSendFrame.data(), pendingSendFrame.data() + canFrameSize);
        streamWriter = nullptr;

        if (pendingSendOnDone)
        {
            auto onDone = pendingSendOnDone;
            pendingSendOnDone = nullptr;
            onDone(true);
        }
    }

    void TcpClientCanbus::ConnectionHandler::DataReceived()
    {
        auto stream = services::ConnectionObserver::Subject().ReceiveStream();
        infra::DataInputStream::WithErrorPolicy dataStream(*stream, infra::noFail);

        while (!dataStream.Empty())
        {
            auto bytesToRead = std::min(canFrameSize - accumulatedBytes, static_cast<std::size_t>(dataStream.Available()));
            infra::ByteRange range(receiveAccumulator.data() + accumulatedBytes, receiveAccumulator.data() + accumulatedBytes + bytesToRead);
            dataStream >> range;
            accumulatedBytes += bytesToRead;

            if (accumulatedBytes >= canFrameSize)
            {
                Id id = Id::Create11BitId(0);
                Message msg;
                if (DecodeFrame(receiveAccumulator.data(), id, msg) && parent.receiveCallback)
                    parent.receiveCallback(id, msg);

                accumulatedBytes = 0;
            }
        }

        services::ConnectionObserver::Subject().AckReceived();
    }

    void TcpClientCanbus::ConnectionHandler::RequestSend(Id id, const Message& data, const infra::Function<void(bool success)>& onDone)
    {
        if (pendingSendOnDone)
        {
            if (onDone)
                onDone(false);
            return;
        }

        const auto maxSendStreamSize = services::ConnectionObserver::Subject().MaxSendStreamSize();
        if (maxSendStreamSize < canFrameSize)
        {
            if (onDone)
                onDone(false);
            return;
        }

        EncodeFrame(id, data, pendingSendFrame);
        pendingSendOnDone = onDone;
        services::ConnectionObserver::Subject().RequestSendStream(canFrameSize);
    }

    void TcpClientCanbus::ConnectionHandler::Close()
    {
        parent.HandleDisconnected(false);
    }

    void TcpClientCanbus::ConnectionHandler::Abort()
    {
        parent.HandleDisconnected(false);
    }

    void TcpClientCanbus::ConnectionHandler::FailPendingSend(bool success)
    {
        if (pendingSendOnDone)
        {
            auto onDone = pendingSendOnDone;
            pendingSendOnDone = nullptr;
            onDone(success);
        }
    }

    void TcpClientCanbus::HandleDisconnected(bool clearConnectionHandler)
    {
        connected = false;

        if (connectionHandler)
            connectionHandler->FailPendingSend(false);

        if (clearConnectionHandler)
            connectionHandler = nullptr;
    }

    void TcpClientCanbus::EncodeFrame(Id id, const Message& data, std::array<uint8_t, canFrameSize>& buffer)
    {
        std::memset(buffer.data(), 0, canFrameSize);

        uint32_t canId{ 0 };
        if (id.Is29BitId())
            canId = id.Get29BitId() | canEffFlag;
        else
            canId = id.Get11BitId();

        std::memcpy(buffer.data(), &canId, sizeof(uint32_t));
        buffer[4] = static_cast<uint8_t>(std::min(data.size(), std::size_t{ 8 }));
        std::memcpy(buffer.data() + 8, data.data(), std::min(data.size(), std::size_t{ 8 }));
    }

    bool TcpClientCanbus::DecodeFrame(const uint8_t* raw, Id& outId, Message& outData)
    {
        uint32_t canId{ 0 };
        std::memcpy(&canId, raw, sizeof(uint32_t));

        bool isExtended = (canId & canEffFlag) != 0;
        uint32_t arbId = canId & 0x1FFFFFFF;

        if (isExtended)
            outId = Id::Create29BitId(arbId);
        else
            outId = Id::Create11BitId(arbId);

        uint8_t dlc = std::min(raw[4], uint8_t{ 8 });
        outData.clear();
        for (uint8_t i = 0; i < dlc; ++i)
            outData.push_back(raw[8 + i]);

        return true;
    }
}
