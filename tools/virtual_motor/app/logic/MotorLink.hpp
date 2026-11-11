#pragma once

#include "tools/virtual_motor/port/PlantPortCodec.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    class MotorLinkObserver
    {
    public:
        virtual void StatusChanged(const port::Status& status) = 0;
        virtual void ChangesChanged(const port::Changes& changes) = 0;
        virtual void RequestRefused(const port::Refused& refused) = 0;
        virtual void LogLine(const std::string& line) = 0;
        virtual void WindowReceived(const port::Window& window) = 0;
        virtual void CaptureEventReceived(const port::CaptureEvent& event) = 0;
        virtual void CaptureChunkReceived(const port::CaptureChunk& chunk) = 0;
        virtual void ProfileStateReceived(const port::ProfileState& state) = 0;
        virtual void LinkClosed() = 0;

    protected:
        ~MotorLinkObserver() = default;
    };

    class MotorLink
    {
    public:
        explicit MotorLink(intptr_t socket);
        MotorLink(const MotorLink&) = delete;
        MotorLink& operator=(const MotorLink&) = delete;
        ~MotorLink();

        void Observe(MotorLinkObserver& observer);
        void Poll();
        bool IsOpen() const;

        const std::optional<port::Hello>& Identity() const;
        const std::optional<port::Status>& LastStatus() const;
        const std::optional<port::Changes>& LastChanges() const;

        template<class Request>
        void Send(const Request& request)
        {
            SendBytes(port::Encode(request));
        }

    private:
        void Dispatch(const port::Frame& frame);
        bool DispatchInstrument(const port::Frame& frame);
        void SendBytes(const std::vector<uint8_t>& bytes);
        void Close();

        intptr_t socket;
        port::FrameAssembler assembler;
        MotorLinkObserver* observer{ nullptr };
        std::optional<port::Hello> hello;
        std::optional<port::Status> status;
        std::optional<port::Changes> changes;
        std::vector<std::string> earlyLog;
    };
}
