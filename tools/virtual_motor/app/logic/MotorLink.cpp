#include "tools/virtual_motor/app/logic/MotorLink.hpp"
#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include <array>

namespace virtual_motor::app
{
    MotorLink::MotorLink(intptr_t socket)
        : socket(socket)
    {}

    MotorLink::~MotorLink()
    {
        services::loopback::Close(socket);
    }

    void MotorLink::Observe(MotorLinkObserver& linkObserver)
    {
        observer = &linkObserver;
        for (const auto& line : earlyLog)
            observer->LogLine(line);
        earlyLog.clear();
    }

    void MotorLink::Poll()
    {
        std::array<uint8_t, 65536> buffer{};
        while (IsOpen())
        {
            const auto size = services::loopback::Receive(socket, infra::ByteRange{ buffer.data(), buffer.data() + buffer.size() });
            if (!size.has_value())
                Close();
            else if (*size == 0)
                break;
            else
                assembler.Append(buffer.data(), *size);
        }

        while (auto frame = assembler.Next())
            Dispatch(*frame);
    }

    bool MotorLink::IsOpen() const
    {
        return socket != services::loopback::invalidSocket;
    }

    const std::optional<port::Hello>& MotorLink::Identity() const
    {
        return hello;
    }

    const std::optional<port::Status>& MotorLink::LastStatus() const
    {
        return status;
    }

    const std::optional<port::Changes>& MotorLink::LastChanges() const
    {
        return changes;
    }

    void MotorLink::Dispatch(const port::Frame& frame)
    {
        if (auto identity = port::Decode<port::Hello>(frame))
            hello = identity;
        else if (auto update = port::Decode<port::Status>(frame))
        {
            status = update;
            if (observer != nullptr)
                observer->StatusChanged(*update);
        }
        else if (auto inForce = port::Decode<port::Changes>(frame))
        {
            changes = inForce;
            if (observer != nullptr)
                observer->ChangesChanged(*inForce);
        }
        else if (auto refused = port::Decode<port::Refused>(frame); refused && observer != nullptr)
            observer->RequestRefused(*refused);
        else if (auto line = port::Decode<port::LogLine>(frame))
        {
            if (observer != nullptr)
                observer->LogLine(line->text);
            else
                earlyLog.push_back(line->text);
        }
    }

    void MotorLink::SendBytes(const std::vector<uint8_t>& bytes)
    {
        if (IsOpen() && !services::loopback::Send(socket, infra::ConstByteRange{ bytes.data(), bytes.data() + bytes.size() }))
            Close();
    }

    void MotorLink::Close()
    {
        services::loopback::Close(socket);
        socket = services::loopback::invalidSocket;
        if (observer != nullptr)
            observer->LinkClosed();
    }
}
