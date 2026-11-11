#include "tools/virtual_motor/platform/PlantPortClient.hpp"
#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/platform/PortSocket.hpp"
#include <array>

namespace virtual_motor
{
    namespace
    {
        constexpr uint32_t statusIntervalMilliseconds{ 100 };

        template<class Request>
        std::optional<Request> Read(const port::Frame& frame, bool& valid)
        {
            auto request = port::Decode<Request>(frame);
            valid = request.has_value();
            return request;
        }
    }

    PlantPortClient::PlantPortClient(PlantPortTarget& target)
        : target(target)
        , socket(services::loopback::invalidSocket)
    {}

    PlantPortClient::~PlantPortClient()
    {
        services::loopback::Close(socket);
    }

    bool PlantPortClient::Connect(uint16_t port)
    {
        socket = services::loopback::Connect(port);
        if (socket == services::loopback::invalidSocket)
            return false;

        Send(port::Encode(target.Identity()));
        Send(port::Encode(target.ChangesInForce()));
        PublishStatus();
        target.Attach(*this);
        return IsConnected();
    }

    bool PlantPortClient::IsConnected() const
    {
        return socket != services::loopback::invalidSocket;
    }

    void PlantPortClient::Poll()
    {
        Flush();

        std::array<uint8_t, 1024> buffer{};
        while (IsConnected())
        {
            const auto size = services::loopback::Receive(socket, infra::ByteRange{ buffer.data(), buffer.data() + buffer.size() });
            if (!size.has_value())
                Disconnect();
            else if (*size == 0)
                break;
            else
                assembler.Append(buffer.data(), *size);
        }

        while (IsConnected())
        {
            auto frame = assembler.Next();
            if (!frame)
                break;
            Dispatch(*frame);
        }
    }

    void PlantPortClient::Millisecond()
    {
        if (++millisecondsSinceStatus < statusIntervalMilliseconds)
            return;

        millisecondsSinceStatus = 0;
        PublishStatus();
    }

    void PlantPortClient::PublishStatus()
    {
        Send(port::Encode(target.CurrentStatus()));
    }

    void PlantPortClient::Log(infra::ConstByteRange text)
    {
        for (auto character : text)
        {
            if (character == '\n')
            {
                if (!partialLine.empty() && partialLine.back() == '\r')
                    partialLine.pop_back();
                Send(port::Encode(port::LogLine{ partialLine }));
                partialLine.clear();
            }
            else
                partialLine.push_back(static_cast<char>(character));
        }
    }

    void PlantPortClient::ReportWindow(const port::Window& window)
    {
        auto bytes = port::Encode(window);
        if (outgoing.size() < bytes.size())
            Send(bytes);
    }

    void PlantPortClient::ReportCaptureEvent(const port::CaptureEvent& event)
    {
        Send(port::Encode(event));
    }

    void PlantPortClient::ReportCaptureChunk(const port::CaptureChunk& chunk)
    {
        Send(port::Encode(chunk));
    }

    void PlantPortClient::ReportProfileState(const port::ProfileState& state)
    {
        Send(port::Encode(state));
        Send(port::Encode(target.ChangesInForce()));
    }

    void PlantPortClient::Dispatch(const port::Frame& frame)
    {
        if (!HandleRun(frame) || !HandleChange(frame) || !HandleInstruments(frame))
            Disconnect();
    }

    bool PlantPortClient::HandleInstruments(const port::Frame& frame)
    {
        bool valid{ true };
        switch (static_cast<port::MessageType>(frame.type))
        {
            case port::MessageType::stream:
                return HandleStream(frame);
            case port::MessageType::capture:
                if (auto request = Read<port::CaptureRequest>(frame, valid))
                    if (auto refusal = target.Capture(*request))
                        Answer(port::MessageType::capture, refusal, false);
                break;
            case port::MessageType::cancelCapture:
                target.CancelCapture();
                break;
            case port::MessageType::profile:
                if (auto request = Read<port::ProfileRequest>(frame, valid))
                    if (auto refusal = target.RunProfile(*request))
                        Answer(port::MessageType::profile, refusal, false);
                break;
            case port::MessageType::stopProfile:
                target.StopProfile();
                break;
            default:
                break;
        }
        return valid;
    }

    bool PlantPortClient::HandleStream(const port::Frame& frame)
    {
        bool valid{ false };
        if (auto request = Read<port::StreamRequest>(frame, valid))
        {
            if (request->on > 1)
                Answer(port::MessageType::stream, port::RefusalReason::outOfRange, false);
            else
                target.Stream(request->on == 1);
        }
        return valid;
    }

    bool PlantPortClient::HandleRun(const port::Frame& frame)
    {
        switch (static_cast<port::MessageType>(frame.type))
        {
            case port::MessageType::pause:
                target.Pause();
                PublishStatus();
                return true;
            case port::MessageType::resume:
                target.Resume();
                PublishStatus();
                return true;
            case port::MessageType::step:
            {
                bool valid{ false };
                if (auto step = Read<port::Step>(frame, valid))
                    Answer(port::MessageType::step, target.Step(step->unit, step->count), true);
                return valid;
            }
            default:
                return true;
        }
    }

    bool PlantPortClient::HandleChange(const port::Frame& frame)
    {
        bool valid{ true };
        switch (static_cast<port::MessageType>(frame.type))
        {
            case port::MessageType::loadTorque:
                if (auto request = Read<port::LoadTorque>(frame, valid))
                    Answer(port::MessageType::loadTorque, target.SetLoadTorque(request->newtonMeter), false);
                break;
            case port::MessageType::externalTorque:
                if (auto request = Read<port::ExternalTorqueRequest>(frame, valid))
                    Answer(port::MessageType::externalTorque, target.SetExternalTorque(request->torque), false);
                break;
            case port::MessageType::currentNoise:
                if (auto request = Read<port::CurrentNoise>(frame, valid))
                    Answer(port::MessageType::currentNoise, target.SetCurrentNoise(request->ampere), false);
                break;
            case port::MessageType::encoderNoise:
                if (auto request = Read<port::EncoderNoise>(frame, valid))
                    Answer(port::MessageType::encoderNoise, target.SetEncoderNoise(request->radians), false);
                break;
            case port::MessageType::clearAll:
                target.ClearAll();
                Answer(port::MessageType::clearAll, std::nullopt, false);
                break;
            default:
                break;
        }
        return valid;
    }

    void PlantPortClient::Answer(port::MessageType request, std::optional<port::RefusalReason> refusal, bool withStatus)
    {
        if (refusal)
            Send(port::Encode(port::Refused{ static_cast<uint8_t>(request), *refusal }));
        else if (withStatus)
            PublishStatus();
        else
            Send(port::Encode(target.ChangesInForce()));
    }

    void PlantPortClient::Send(const std::vector<uint8_t>& bytes)
    {
        if (!IsConnected())
            return;

        outgoing.insert(outgoing.end(), bytes.begin(), bytes.end());
        Flush();
    }

    void PlantPortClient::Flush()
    {
        while (IsConnected() && !outgoing.empty())
        {
            const auto sent = SendWithoutWaiting(socket, infra::ConstByteRange{ outgoing.data(), outgoing.data() + outgoing.size() });
            if (!sent.has_value())
                Disconnect();
            else if (*sent == 0)
                break;
            else
                outgoing.erase(outgoing.begin(), outgoing.begin() + static_cast<std::ptrdiff_t>(*sent));
        }
    }

    void PlantPortClient::Disconnect()
    {
        services::loopback::Close(socket);
        socket = services::loopback::invalidSocket;
        partialLine.clear();
        outgoing.clear();
        target.Detach();
        target.Resume();
    }
}
