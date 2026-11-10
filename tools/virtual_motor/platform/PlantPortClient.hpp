#pragma once

#include "infra/util/ByteRange.hpp"
#include "tools/virtual_motor/port/PlantPortCodec.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace virtual_motor
{
    class PlantPortTarget
    {
    public:
        virtual port::Hello Identity() const = 0;
        virtual port::Status CurrentStatus() const = 0;
        virtual port::Changes ChangesInForce() const = 0;

        virtual void Pause() = 0;
        virtual void Resume() = 0;
        virtual std::optional<port::RefusalReason> Step(port::StepUnit unit, uint16_t count) = 0;

        virtual std::optional<port::RefusalReason> SetLoadTorque(float newtonMeter) = 0;
        virtual std::optional<port::RefusalReason> SetExternalTorque(const port::ExternalTorque& torque) = 0;
        virtual std::optional<port::RefusalReason> SetCurrentNoise(float ampere) = 0;
        virtual std::optional<port::RefusalReason> SetEncoderNoise(float radians) = 0;
        virtual void ClearAll() = 0;

    protected:
        ~PlantPortTarget() = default;
    };

    class PlantPortClient
    {
    public:
        explicit PlantPortClient(PlantPortTarget& target);
        PlantPortClient(const PlantPortClient&) = delete;
        PlantPortClient& operator=(const PlantPortClient&) = delete;
        ~PlantPortClient();

        bool Connect(uint16_t port);
        bool IsConnected() const;

        void Poll();
        void Millisecond();
        void PublishStatus();
        void Log(infra::ConstByteRange text);

    private:
        void Dispatch(const port::Frame& frame);
        bool HandleRun(const port::Frame& frame);
        bool HandleChange(const port::Frame& frame);
        void Answer(port::MessageType request, std::optional<port::RefusalReason> refusal, bool withStatus);
        void Send(const std::vector<uint8_t>& bytes);
        void Disconnect();

        PlantPortTarget& target;
        intptr_t socket;
        port::FrameAssembler assembler;
        std::string partialLine;
        uint32_t millisecondsSinceStatus{ 0 };
    };
}
