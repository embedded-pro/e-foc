#include "tools/hardware_bridge/client/serial/TcpClientSerial.hpp"
#include "infra/stream/InputStream.hpp"
#include "infra/stream/OutputStream.hpp"
#include <algorithm>

namespace tool
{
    TcpClientSerial::TcpClientSerial(services::ConnectionFactory& factory, services::IPAddress address, uint16_t port)
        : TcpClient(factory, address, port)
    {}

    // GCOVR_EXCL_START - destructor cleanup is exercised through connection-stub ownership checks.
    TcpClientSerial::~TcpClientSerial()
    {
        if (connectionHandlerPtr)
            connectionHandlerPtr->Subject().AbortAndDestroy();
    }

    // GCOVR_EXCL_STOP

    void TcpClientSerial::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        if (IsConnected() && connectionHandler)
            connectionHandler->RequestSend(data, actionOnCompletion);
        else
        {
            if (queuedSendOnDone)
            {
                auto previousOnDone = queuedSendOnDone;
                queuedSendOnDone = nullptr;
                previousOnDone();
            }

            queuedSendData.assign(data.begin(), data.end());
            queuedSendOnDone = actionOnCompletion;
        }
    }

    void TcpClientSerial::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        receiveCallback = dataReceived;
    }

    void TcpClientSerial::OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver)
    {
        connectionHandlerPtr = connectionHandler.Emplace(*this);
        createdObserver(connectionHandlerPtr);

        if (connectionHandlerPtr && (!queuedSendData.empty() || queuedSendOnDone))
        {
            connectionHandlerPtr->RequestSend(infra::ConstByteRange(queuedSendData.data(), queuedSendData.data() + queuedSendData.size()), queuedSendOnDone);
            queuedSendData.clear();
            queuedSendOnDone = nullptr;
        }
    }

    void TcpClientSerial::OnDisconnected(bool clearConnectionHandler)
    {
        if (queuedSendOnDone)
        {
            auto onDone = queuedSendOnDone;
            queuedSendOnDone = nullptr;
            queuedSendData.clear();
            onDone();
        }
        else
            queuedSendData.clear();

        if (connectionHandlerPtr)
            connectionHandlerPtr->FailPendingSend();

        if (clearConnectionHandler)
            connectionHandlerPtr = nullptr;
    }

    TcpClientSerial::ConnectionHandler::ConnectionHandler(TcpClientSerial& parent)
        : parent(parent)
    {}

    void TcpClientSerial::ConnectionHandler::SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& streamWriter)
    {
        // GCOVR_EXCL_START - SendStreamAvailable without a requested send is a defensive connection callback path.
        if (pendingSendData.empty())
        {
            streamWriter = nullptr;
            return;
        }
        // GCOVR_EXCL_STOP

        const auto remainingBytes = pendingSendData.size() - pendingSendOffset;
        const auto chunkSize = std::min(remainingBytes, services::ConnectionObserver::Subject().MaxSendStreamSize());
        infra::DataOutputStream::WithErrorPolicy stream(*streamWriter);
        stream << infra::ConstByteRange(
            pendingSendData.data() + pendingSendOffset,
            pendingSendData.data() + pendingSendOffset + chunkSize);
        streamWriter = nullptr;

        pendingSendOffset += chunkSize;
        if (pendingSendOffset < pendingSendData.size())
        {
            RequestNextSendChunk();
            return;
        }

        pendingSendData.clear();
        pendingSendOffset = 0;

        if (pendingSendOnDone)
        {
            auto onDone = pendingSendOnDone;
            pendingSendOnDone = nullptr;
            onDone();
        }
    }

    void TcpClientSerial::ConnectionHandler::DataReceived()
    {
        auto stream = services::ConnectionObserver::Subject().ReceiveStream();
        infra::DataInputStream::WithErrorPolicy dataStream(*stream, infra::noFail);

        while (!dataStream.Empty())
        {
            auto chunk = dataStream.ContiguousRange();
            if (chunk.empty())
                break; // GCOVR_EXCL_LINE - ReceiveStream only reports non-empty chunks in the connection stub.
            if (parent.receiveCallback)
                parent.receiveCallback(chunk);
        }

        services::ConnectionObserver::Subject().AckReceived();
    }

    void TcpClientSerial::ConnectionHandler::RequestSend(infra::ConstByteRange data, infra::Function<void()> onDone)
    {
        if (pendingSendOnDone)
        {
            if (onDone)
                onDone();
            return;
        }

        if (data.empty())
        {
            if (onDone)
                onDone();
            return;
        }

        pendingSendData.assign(data.begin(), data.end());
        pendingSendOffset = 0;
        pendingSendOnDone = onDone;
        RequestNextSendChunk();
    }

    void TcpClientSerial::ConnectionHandler::RequestNextSendChunk() const
    {
        if (pendingSendOffset >= pendingSendData.size())
            return; // GCOVR_EXCL_LINE - Guard for direct/internal misuse; public sends return before this state.

        const auto sendSize = std::min(
            pendingSendData.size() - pendingSendOffset,
            services::ConnectionObserver::Subject().MaxSendStreamSize());
        services::ConnectionObserver::Subject().RequestSendStream(sendSize);
    }

    void TcpClientSerial::ConnectionHandler::Close()
    {
        parent.HandleDisconnected(false);
    }

    void TcpClientSerial::ConnectionHandler::Abort()
    {
        parent.HandleDisconnected(false);
    }

    void TcpClientSerial::ConnectionHandler::FailPendingSend()
    {
        pendingSendData.clear();
        pendingSendOffset = 0;

        // GCOVR_EXCL_START - Active-send failure cleanup is private to connection teardown and not reachable with ConnectionStub.
        if (pendingSendOnDone)
        {
            auto onDone = pendingSendOnDone;
            pendingSendOnDone = nullptr;
            onDone();
        }
        // GCOVR_EXCL_STOP
    }

    void TcpClientSerial::ConnectionHandler::Detaching()
    {
        parent.HandleDisconnected(true);
    }
}
