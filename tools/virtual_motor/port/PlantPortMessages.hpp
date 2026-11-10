#pragma once

#include "tools/virtual_motor/port/PlantPortPayload.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace virtual_motor::port
{
    constexpr uint16_t protocolVersion{ 1 };

    enum class MessageType : uint8_t
    {
        hello = 0x01,
        status = 0x02,
        changes = 0x03,
        refused = 0x04,
        logLine = 0x05,
        pause = 0x81,
        resume = 0x82,
        step = 0x83,
        loadTorque = 0x84,
        externalTorque = 0x85,
        currentNoise = 0x86,
        encoderNoise = 0x87,
        clearAll = 0x88,
    };

    enum class RunState : uint8_t
    {
        running,
        paused,
    };

    enum class TorqueForm : uint8_t
    {
        none,
        constant,
        step,
        ramp,
        sine,
    };

    enum class RefusalReason : uint8_t
    {
        outOfRange,
        stepWhileRunning,
        unknownForm,
    };

    enum class StepUnit : uint8_t
    {
        controlPeriod,
        millisecond,
    };

    namespace led
    {
        constexpr uint8_t operational{ 0b001 };
        constexpr uint8_t warning{ 0b010 };
        constexpr uint8_t failure{ 0b100 };
    }

    namespace differs
    {
        constexpr uint8_t loadTorque{ 0b0001 };
        constexpr uint8_t externalTorque{ 0b0010 };
        constexpr uint8_t currentNoise{ 0b0100 };
        constexpr uint8_t encoderNoise{ 0b1000 };
    }

    struct ExternalTorque
    {
        TorqueForm form{ TorqueForm::none };
        std::array<float, 4> parameters{};

        void WriteTo(PayloadWriter& writer) const;
        static ExternalTorque ReadFrom(PayloadReader& reader);
    };

    struct Hello
    {
        static constexpr MessageType type{ MessageType::hello };

        uint16_t version{ protocolVersion };
        uint8_t node{ 0 };
        std::string bus;
        std::string plant;
        uint32_t controlRateHz{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static Hello ReadFrom(PayloadReader& reader);
    };

    struct Status
    {
        static constexpr MessageType type{ MessageType::status };

        uint64_t controlPeriods{ 0 };
        float realTimeFactor{ 0.0f };
        uint32_t overruns{ 0 };
        RunState runState{ RunState::running };
        uint8_t leds{ 0 };
        uint8_t protection{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static Status ReadFrom(PayloadReader& reader);
    };

    struct Changes
    {
        static constexpr MessageType type{ MessageType::changes };

        float loadTorque{ 0.0f };
        ExternalTorque externalTorque;
        float currentNoise{ 0.0f };
        float encoderNoise{ 0.0f };
        uint8_t differs{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static Changes ReadFrom(PayloadReader& reader);
    };

    struct Refused
    {
        static constexpr MessageType type{ MessageType::refused };

        uint8_t request{ 0 };
        RefusalReason reason{ RefusalReason::outOfRange };

        void WriteTo(PayloadWriter& writer) const;
        static Refused ReadFrom(PayloadReader& reader);
    };

    struct LogLine
    {
        static constexpr MessageType type{ MessageType::logLine };

        std::string text;

        void WriteTo(PayloadWriter& writer) const;
        static LogLine ReadFrom(PayloadReader& reader);
    };

    template<MessageType messageType>
    struct EmptyRequest
    {
        static constexpr MessageType type{ messageType };

        void WriteTo(PayloadWriter&) const
        {}

        static EmptyRequest ReadFrom(PayloadReader&)
        {
            return {};
        }
    };

    using Pause = EmptyRequest<MessageType::pause>;
    using Resume = EmptyRequest<MessageType::resume>;
    using ClearAll = EmptyRequest<MessageType::clearAll>;

    struct Step
    {
        static constexpr MessageType type{ MessageType::step };

        StepUnit unit{ StepUnit::controlPeriod };
        uint16_t count{ 1 };

        void WriteTo(PayloadWriter& writer) const;
        static Step ReadFrom(PayloadReader& reader);
    };

    struct LoadTorque
    {
        static constexpr MessageType type{ MessageType::loadTorque };

        float newtonMeter{ 0.0f };

        void WriteTo(PayloadWriter& writer) const;
        static LoadTorque ReadFrom(PayloadReader& reader);
    };

    struct ExternalTorqueRequest
    {
        static constexpr MessageType type{ MessageType::externalTorque };

        ExternalTorque torque;

        void WriteTo(PayloadWriter& writer) const;
        static ExternalTorqueRequest ReadFrom(PayloadReader& reader);
    };

    struct CurrentNoise
    {
        static constexpr MessageType type{ MessageType::currentNoise };

        float ampere{ 0.0f };

        void WriteTo(PayloadWriter& writer) const;
        static CurrentNoise ReadFrom(PayloadReader& reader);
    };

    struct EncoderNoise
    {
        static constexpr MessageType type{ MessageType::encoderNoise };

        float radians{ 0.0f };

        void WriteTo(PayloadWriter& writer) const;
        static EncoderNoise ReadFrom(PayloadReader& reader);
    };
}
