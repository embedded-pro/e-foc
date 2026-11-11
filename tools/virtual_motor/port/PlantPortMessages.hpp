#pragma once

#include "tools/virtual_motor/port/PlantPortPayload.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace virtual_motor::port
{
    constexpr uint16_t protocolVersion{ 2 };
    constexpr std::size_t signalCount{ 25 };
    constexpr uint16_t maxCapturePeriods{ 16384 };
    constexpr uint16_t captureChunkPeriods{ 512 };
    constexpr uint16_t maxProfileSteps{ 1024 };

    using Sample = std::array<float, signalCount>;

    enum class MessageType : uint8_t
    {
        hello = 0x01,
        status = 0x02,
        changes = 0x03,
        refused = 0x04,
        logLine = 0x05,
        window = 0x10,
        captureEvent = 0x11,
        captureChunk = 0x12,
        profileState = 0x13,
        pause = 0x81,
        resume = 0x82,
        step = 0x83,
        loadTorque = 0x84,
        externalTorque = 0x85,
        currentNoise = 0x86,
        encoderNoise = 0x87,
        clearAll = 0x88,
        stream = 0x90,
        capture = 0x91,
        cancelCapture = 0x92,
        profile = 0x93,
        stopProfile = 0x94,
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

    enum class TriggerKind : uint8_t
    {
        immediately,
        rising,
        falling,
        either,
        protection,
        leds,
    };

    enum class CaptureEventKind : uint8_t
    {
        armed,
        triggered,
        cancelled,
    };

    enum class ProfileStateKind : uint8_t
    {
        armed,
        running,
        finished,
        stopped,
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
    using CancelCapture = EmptyRequest<MessageType::cancelCapture>;
    using StopProfile = EmptyRequest<MessageType::stopProfile>;

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

    struct Window
    {
        static constexpr MessageType type{ MessageType::window };

        uint32_t sequence{ 0 };
        uint64_t firstPeriod{ 0 };
        uint8_t periods{ 0 };
        std::array<float, signalCount> minimum{};
        std::array<float, signalCount> maximum{};
        std::array<float, signalCount> last{};

        void WriteTo(PayloadWriter& writer) const;
        static Window ReadFrom(PayloadReader& reader);
    };

    struct StreamRequest
    {
        static constexpr MessageType type{ MessageType::stream };

        uint8_t on{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static StreamRequest ReadFrom(PayloadReader& reader);
    };

    struct Trigger
    {
        TriggerKind kind{ TriggerKind::immediately };
        uint8_t signal{ 0 };
        float level{ 0.0f };
        uint8_t leds{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static Trigger ReadFrom(PayloadReader& reader);
    };

    struct CaptureRequest
    {
        static constexpr MessageType type{ MessageType::capture };

        Trigger trigger;
        uint8_t preTriggerPercent{ 0 };
        uint16_t length{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static CaptureRequest ReadFrom(PayloadReader& reader);
    };

    struct CaptureEvent
    {
        static constexpr MessageType type{ MessageType::captureEvent };

        uint16_t capture{ 0 };
        CaptureEventKind event{ CaptureEventKind::armed };
        uint64_t period{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static CaptureEvent ReadFrom(PayloadReader& reader);
    };

    struct CaptureChunk
    {
        static constexpr MessageType type{ MessageType::captureChunk };

        uint16_t capture{ 0 };
        uint16_t length{ 0 };
        uint16_t preTrigger{ 0 };
        uint64_t firstPeriod{ 0 };
        uint16_t index{ 0 };
        std::vector<Sample> periods;

        void WriteTo(PayloadWriter& writer) const;
        static CaptureChunk ReadFrom(PayloadReader& reader);
    };

    struct ProfileStep
    {
        uint32_t at{ 0 };
        MessageType change{ MessageType::loadTorque };
        float value{ 0.0f };
        ExternalTorque torque;
    };

    struct ProfileRequest
    {
        static constexpr MessageType type{ MessageType::profile };

        Trigger trigger;
        std::vector<ProfileStep> steps;
        bool readable{ true };

        void WriteTo(PayloadWriter& writer) const;
        static ProfileRequest ReadFrom(PayloadReader& reader);
    };

    struct ProfileState
    {
        static constexpr MessageType type{ MessageType::profileState };

        ProfileStateKind state{ ProfileStateKind::armed };
        uint16_t applied{ 0 };
        uint16_t total{ 0 };
        uint32_t elapsed{ 0 };

        void WriteTo(PayloadWriter& writer) const;
        static ProfileState ReadFrom(PayloadReader& reader);
    };
}
