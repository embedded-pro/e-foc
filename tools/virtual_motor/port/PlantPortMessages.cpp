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
}
