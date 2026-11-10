#include "tools/virtual_motor/port/PlantPortMessages.hpp"

namespace virtual_motor::port
{
    void ExternalTorque::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(static_cast<uint8_t>(form));
        for (auto parameter : parameters)
            writer.F32(parameter);
    }

    ExternalTorque ExternalTorque::ReadFrom(PayloadReader& reader)
    {
        ExternalTorque torque{ static_cast<TorqueForm>(reader.U8()) };
        for (auto& parameter : torque.parameters)
            parameter = reader.F32();
        return torque;
    }

    void Hello::WriteTo(PayloadWriter& writer) const
    {
        writer.U16(version);
        writer.U8(node);
        writer.Text(bus);
        writer.Text(plant);
        writer.U32(controlRateHz);
    }

    Hello Hello::ReadFrom(PayloadReader& reader)
    {
        Hello hello;
        hello.version = reader.U16();
        hello.node = reader.U8();
        hello.bus = reader.Text();
        hello.plant = reader.Text();
        hello.controlRateHz = reader.U32();
        return hello;
    }

    void Status::WriteTo(PayloadWriter& writer) const
    {
        writer.U64(controlPeriods);
        writer.F32(realTimeFactor);
        writer.U32(overruns);
        writer.U8(static_cast<uint8_t>(runState));
        writer.U8(leds);
        writer.U8(protection);
    }

    Status Status::ReadFrom(PayloadReader& reader)
    {
        Status status;
        status.controlPeriods = reader.U64();
        status.realTimeFactor = reader.F32();
        status.overruns = reader.U32();
        status.runState = static_cast<RunState>(reader.U8());
        status.leds = reader.U8();
        status.protection = reader.U8();
        return status;
    }

    void Changes::WriteTo(PayloadWriter& writer) const
    {
        writer.F32(loadTorque);
        externalTorque.WriteTo(writer);
        writer.F32(currentNoise);
        writer.F32(encoderNoise);
        writer.U8(differs);
    }

    Changes Changes::ReadFrom(PayloadReader& reader)
    {
        Changes changes;
        changes.loadTorque = reader.F32();
        changes.externalTorque = ExternalTorque::ReadFrom(reader);
        changes.currentNoise = reader.F32();
        changes.encoderNoise = reader.F32();
        changes.differs = reader.U8();
        return changes;
    }

    void Refused::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(request);
        writer.U8(static_cast<uint8_t>(reason));
    }

    Refused Refused::ReadFrom(PayloadReader& reader)
    {
        Refused refused;
        refused.request = reader.U8();
        refused.reason = static_cast<RefusalReason>(reader.U8());
        return refused;
    }

    void LogLine::WriteTo(PayloadWriter& writer) const
    {
        writer.Text(text);
    }

    LogLine LogLine::ReadFrom(PayloadReader& reader)
    {
        return { reader.Text() };
    }

    void Step::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(static_cast<uint8_t>(unit));
        writer.U16(count);
    }

    Step Step::ReadFrom(PayloadReader& reader)
    {
        Step step;
        step.unit = static_cast<StepUnit>(reader.U8());
        step.count = reader.U16();
        return step;
    }

    void LoadTorque::WriteTo(PayloadWriter& writer) const
    {
        writer.F32(newtonMeter);
    }

    LoadTorque LoadTorque::ReadFrom(PayloadReader& reader)
    {
        return { reader.F32() };
    }

    void ExternalTorqueRequest::WriteTo(PayloadWriter& writer) const
    {
        torque.WriteTo(writer);
    }

    ExternalTorqueRequest ExternalTorqueRequest::ReadFrom(PayloadReader& reader)
    {
        return { ExternalTorque::ReadFrom(reader) };
    }

    void CurrentNoise::WriteTo(PayloadWriter& writer) const
    {
        writer.F32(ampere);
    }

    CurrentNoise CurrentNoise::ReadFrom(PayloadReader& reader)
    {
        return { reader.F32() };
    }

    void EncoderNoise::WriteTo(PayloadWriter& writer) const
    {
        writer.F32(radians);
    }

    EncoderNoise EncoderNoise::ReadFrom(PayloadReader& reader)
    {
        return { reader.F32() };
    }

    void Window::WriteTo(PayloadWriter& writer) const
    {
        writer.U32(sequence);
        writer.U64(firstPeriod);
        writer.U8(periods);
        for (std::size_t signal = 0; signal != signalCount; ++signal)
        {
            writer.F32(minimum[signal]);
            writer.F32(maximum[signal]);
        }
        for (const auto value : last)
            writer.F32(value);
    }

    Window Window::ReadFrom(PayloadReader& reader)
    {
        Window window;
        window.sequence = reader.U32();
        window.firstPeriod = reader.U64();
        window.periods = reader.U8();
        for (std::size_t signal = 0; signal != signalCount; ++signal)
        {
            window.minimum[signal] = reader.F32();
            window.maximum[signal] = reader.F32();
        }
        for (auto& value : window.last)
            value = reader.F32();
        return window;
    }

    void StreamRequest::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(on);
    }

    StreamRequest StreamRequest::ReadFrom(PayloadReader& reader)
    {
        return { reader.U8() };
    }

    namespace
    {
        bool CarriesTorque(MessageType change)
        {
            return change == MessageType::externalTorque;
        }

        bool IsChange(MessageType change)
        {
            return change == MessageType::loadTorque || change == MessageType::externalTorque || change == MessageType::currentNoise || change == MessageType::encoderNoise;
        }
    }

    void Trigger::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(static_cast<uint8_t>(kind));
        writer.U8(signal);
        writer.F32(level);
        writer.U8(leds);
    }

    Trigger Trigger::ReadFrom(PayloadReader& reader)
    {
        Trigger trigger;
        trigger.kind = static_cast<TriggerKind>(reader.U8());
        trigger.signal = reader.U8();
        trigger.level = reader.F32();
        trigger.leds = reader.U8();
        return trigger;
    }

    void CaptureRequest::WriteTo(PayloadWriter& writer) const
    {
        trigger.WriteTo(writer);
        writer.U8(preTriggerPercent);
        writer.U16(length);
    }

    CaptureRequest CaptureRequest::ReadFrom(PayloadReader& reader)
    {
        CaptureRequest request;
        request.trigger = Trigger::ReadFrom(reader);
        request.preTriggerPercent = reader.U8();
        request.length = reader.U16();
        return request;
    }

    void CaptureEvent::WriteTo(PayloadWriter& writer) const
    {
        writer.U16(capture);
        writer.U8(static_cast<uint8_t>(event));
        writer.U64(period);
    }

    CaptureEvent CaptureEvent::ReadFrom(PayloadReader& reader)
    {
        CaptureEvent event;
        event.capture = reader.U16();
        event.event = static_cast<CaptureEventKind>(reader.U8());
        event.period = reader.U64();
        return event;
    }

    void CaptureChunk::WriteTo(PayloadWriter& writer) const
    {
        writer.U16(capture);
        writer.U16(length);
        writer.U16(preTrigger);
        writer.U64(firstPeriod);
        writer.U16(index);
        writer.U16(static_cast<uint16_t>(periods.size()));
        for (const auto& period : periods)
            for (auto value : period)
                writer.F32(value);
    }

    CaptureChunk CaptureChunk::ReadFrom(PayloadReader& reader)
    {
        CaptureChunk chunk;
        chunk.capture = reader.U16();
        chunk.length = reader.U16();
        chunk.preTrigger = reader.U16();
        chunk.firstPeriod = reader.U64();
        chunk.index = reader.U16();
        const auto count = reader.U16();
        for (uint16_t period = 0; period != count && reader.Valid(); ++period)
        {
            auto& sample = chunk.periods.emplace_back();
            for (auto& value : sample)
                value = reader.F32();
        }
        return chunk;
    }

    void ProfileRequest::WriteTo(PayloadWriter& writer) const
    {
        trigger.WriteTo(writer);
        writer.U16(static_cast<uint16_t>(steps.size()));
        for (const auto& step : steps)
        {
            writer.U32(step.at);
            writer.U8(static_cast<uint8_t>(step.change));
            if (CarriesTorque(step.change))
                step.torque.WriteTo(writer);
            else
                writer.F32(step.value);
        }
    }

    ProfileRequest ProfileRequest::ReadFrom(PayloadReader& reader)
    {
        ProfileRequest request;
        request.trigger = Trigger::ReadFrom(reader);
        const auto count = reader.U16();
        for (uint16_t index = 0; index != count && reader.Valid() && request.readable; ++index)
        {
            ProfileStep step{ reader.U32(), static_cast<MessageType>(reader.U8()), 0.0f, {} };
            request.readable = IsChange(step.change);
            if (CarriesTorque(step.change))
                step.torque = ExternalTorque::ReadFrom(reader);
            else if (request.readable)
                step.value = reader.F32();
            request.steps.push_back(step);
        }
        return request;
    }

    void ProfileState::WriteTo(PayloadWriter& writer) const
    {
        writer.U8(static_cast<uint8_t>(state));
        writer.U16(applied);
        writer.U16(total);
        writer.U32(elapsed);
    }

    ProfileState ProfileState::ReadFrom(PayloadReader& reader)
    {
        ProfileState state;
        state.state = static_cast<ProfileStateKind>(reader.U8());
        state.applied = reader.U16();
        state.total = reader.U16();
        state.elapsed = reader.U32();
        return state;
    }
}
