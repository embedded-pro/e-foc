#include "tools/hardware_bridge/client/can/TcpClientCanbus.hpp"
#include "infra/stream/InputStream.hpp"
#include "infra/stream/OutputStream.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include <algorithm>
#include <cstring>

namespace
{
    constexpr std::size_t canFrameSize{ 16 };
    constexpr uint32_t canEffFlag{ 0x80000000 };
    constexpr uint32_t canErrFlag{ 0x20000000 };

    void EncodeFrame(hal::Can::Id id, const hal::Can::Message& data, std::array<uint8_t, canFrameSize>& buffer)
    {
        std::memset(buffer.data(), 0, canFrameSize);

        uint32_t canId{ 0 };
        if (id.Is29BitId())
            canId = id.Get29BitId() | canEffFlag;
        else
            canId = id.Get11BitId();

        buffer[0] = static_cast<uint8_t>(canId & 0xFF);
        buffer[1] = static_cast<uint8_t>((canId >> 8) & 0xFF);
        buffer[2] = static_cast<uint8_t>((canId >> 16) & 0xFF);
        buffer[3] = static_cast<uint8_t>((canId >> 24) & 0xFF);
        buffer[4] = static_cast<uint8_t>(std::min(data.size(), std::size_t{ 8 }));
        std::memcpy(buffer.data() + 8, data.data(), std::min(data.size(), std::size_t{ 8 }));
    }

    bool DecodeFrame(const uint8_t* raw, hal::Can::Id& outId, hal::Can::Message& outData)
    {
        const uint32_t canId = static_cast<uint32_t>(raw[0]) | (static_cast<uint32_t>(raw[1]) << 8) | (static_cast<uint32_t>(raw[2]) << 16) | (static_cast<uint32_t>(raw[3]) << 24);

        if (canId & canErrFlag)
            return false;

        const bool isExtended = (canId & canEffFlag) != 0;
        const uint32_t arbId = canId & 0x1FFFFFFF;

        if (isExtended)
            outId = hal::Can::Id::Create29BitId(arbId);
        else
            outId = hal::Can::Id::Create11BitId(arbId);

        const uint8_t dlc = std::min(raw[4], uint8_t{ 8 });
        outData.clear();
        for (uint8_t i = 0; i < dlc; ++i)
            outData.push_back(raw[8 + i]);

        return true;
    }
}

namespace tool
{
    TcpClientCanbus::TcpClientCanbus(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port)
        : TcpClient(factory, address, port)
    {}

    TcpClientCanbus::~TcpClientCanbus()
    {
        if (connectionHandlerPtr)
            connectionHandlerPtr->Subject().AbortAndDestroy();
    }

    void TcpClientCanbus::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (IsConnected() && connectionHandler)
            connectionHandler->RequestSend(id, data, actionOnCompletion);
        else if (actionOnCompletion)
            actionOnCompletion(false);
    }

    void TcpClientCanbus::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        receiveCallback = receivedAction;
    }

    void TcpClientCanbus::OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver)
    {
        connectionHandlerPtr = connectionHandler.Emplace(*this);
        createdObserver(connectionHandlerPtr);
    }

    void TcpClientCanbus::OnDisconnected(bool clearConnectionHandler)
    {
        if (connectionHandlerPtr)
            connectionHandlerPtr->FailPendingSend(false);

        if (clearConnectionHandler)
            connectionHandlerPtr = nullptr;
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

        auto onDone = pendingSendOnDone;
        pendingSendOnDone = nullptr;
        onDone(true);
    }

    void TcpClientCanbus::ConnectionHandler::DataReceived()
    {
        auto stream = services::ConnectionObserver::Subject().ReceiveStream();
        infra::DataInputStream::WithErrorPolicy dataStream(*stream, infra::noFail);

        while (!dataStream.Empty())
        {
            auto chunk = dataStream.ContiguousRange(canFrameSize - accumulatedBytes);
            if (chunk.empty())
                break;

            std::memcpy(receiveAccumulator.data() + accumulatedBytes, chunk.begin(), chunk.size());
            accumulatedBytes += chunk.size();

            if (accumulatedBytes >= canFrameSize)
            {
                Id id = Id::Create11BitId(0);
                Message msg;
                if (!DecodeFrame(receiveAccumulator.data(), id, msg))
                {
                    services::ConnectionObserver::Subject().AbortAndDestroy();
                    throw BridgeConnectionException("malformed CAN frame");
                }

                if (parent.receiveCallback)
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

    void TcpClientCanbus::ConnectionHandler::Detaching()
    {
        parent.HandleDisconnected(true);
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
}
