#include "targets/hardware_test/components/Terminal.hpp"
#include "foc/interfaces/Driver.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <optional>
#include <ranges>

namespace
{
    constexpr float pi_div_180 = std::numbers::pi_v<float> / 180.0f;
    const std::size_t outerInnerLoopRatio = 10;
    const hal::Hertz defaultPwmFrequency{ 10000 };
    const hal::Hertz speedLoopFrequency{ static_cast<unsigned int>(defaultPwmFrequency.Value() / outerInnerLoopRatio) };

    application::PlatformFactory::SampleAndHold ToSampleAndHold(const infra::BoundedConstString& value)
    {
        using enum application::PlatformFactory::SampleAndHold;
        if (value == "shortest")
            return shortest;
        else if (value == "shorter")
            return shorter;
        else if (value == "medium")
            return medium;
        else if (value == "longer")
            return longer;
        else if (value == "longest")
            return longest;
        else
            std::abort();
    }

    template<typename T>
    std::optional<T> ParseInput(const infra::BoundedConstString& data, T min, T max)
    {
        if (data.empty())
            return {};

        infra::StringInputStream stream(data, infra::softFail);
        T value = 0.0f;
        stream >> value;

        if (!stream.ErrorPolicy().Failed() && value >= min && value <= max)
            return std::make_optional(value);
        else
            return {};
    }

    std::optional<infra::BoundedConstString> ParseInput(const infra::BoundedConstString& data, const infra::BoundedVector<infra::BoundedConstString>& acceptedValues)
    {
        if (data.empty())
            return {};

        auto result = std::ranges::find_if(acceptedValues,
            [&data](const auto& value)
            {
                return data == value;
            });

        if (result != acceptedValues.end())
            return std::make_optional(*result);
        else
            return {};
    }
}

namespace application
{
    using enum services::TerminalWithStorage::Status;

    TerminalInteractor::TerminalInteractor(services::TerminalWithStorage& terminal, application::PlatformFactory& hardware)
        : terminal{ terminal }
        , tracer{ hardware.Tracer() }
        , pwmCreator{ hardware.SynchronousThreeChannelsPwmCreator() }
        , adcCreator{ hardware.AdcMultiChannelCreator() }
        , encoderCreator{ hardware.SynchronousQuadratureEncoderCreator() }
        , canCreator{ hardware.CanBusCreator() }
        , performanceTimer{ hardware.PerformanceTimer() }
        , Vdc{ hardware.PowerSupplyVoltage() }
        , systemClock{ hardware.SystemClock() }
        , foc{ hardware.MaxCurrentSupported(), hal::Hertz{ 1000 }, hardware.LowPriorityInterrupt() }
        , eeprom{ hardware.Eeprom() }
    {
        terminal.AddCommand({ { "enc", "e", "Read encoder. stop. Ex: enc" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(ReadEncoder());
            } });

        terminal.AddCommand({ { "stop", "stp", "Stop pwm. stop. Ex: stop" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(Stop());
            } });

        terminal.AddCommand({ { "duty", "d", "Set and start pwm duty. Ex: duty 0 10 25" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(SetPwmDuty(param));
            } });

        terminal.AddCommand({ { "pwm", "p", "Configure pwm [dead_time ns [500; 2000]] [frequency Hz [10000; 20000]]. Ex: pwm 500 10000" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigurePwm(param));
            } });

        terminal.AddCommand({ { "adc", "a", "Configure adc and prints raw data for all three channels [sample_and_hold [short, medium, long]]. Ex: adc short" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigureAdc(param));
            } });

        terminal.AddCommand({ { "pid", "c", "Configure speed and DQ PIDs [spd_kp spd_ki spd_kd dq_kp dq_ki dq_kd]. Ex: pid 1 0 0 1 0 0" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigurePid(param));
            } });

        terminal.AddCommand({ { "foc", "f", "Simulate foc [angle ia ib ic]. Ex: foc param" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(SimulateFoc(param));
            } });

        terminal.AddCommand({ { "motor", "m", "Set motor parameters [poles [2; 16]]. Ex: motor 14" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(SetMotorParameters(param));
            } });

        terminal.AddCommand({ { "can_start", "cs", "Start CAN bus [bitrate [125000;1000000]] [test]. Ex: can_start 500000" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(CanStart(param));
            } });

        terminal.AddCommand({ { "can_stop", "cx", "Stop CAN bus. Ex: can_stop" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(CanStop());
            } });

        terminal.AddCommand({ { "can_send", "ct", "Send CAN frame [id] [b0] ... [b7]. Ex: can_send 256 1 2 3" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(CanSend(param));
            } });

        terminal.AddCommand({ { "can_listen", "cl", "Listen for CAN messages. Ex: can_listen" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(CanListen());
            } });

        terminal.AddCommand({ { "eeprom_write", "ew", "Write bytes to EEPROM. eeprom_write <addr> <b0> [b1...]. Ex: eeprom_write 0 255 170" },
            [this](const infra::BoundedConstString& param)
            {
                EepromWrite(param);
            } });

        terminal.AddCommand({ { "eeprom_read", "er", "Read bytes from EEPROM. eeprom_read <addr> <size>. Ex: eeprom_read 0 8" },
            [this](const infra::BoundedConstString& param)
            {
                EepromRead(param);
            } });

        terminal.AddCommand({ { "eeprom_erase", "ee", "Erase entire EEPROM. Ex: eeprom_erase" },
            [this](const auto&)
            {
                EepromErase();
            } });

        encoderCreator.Emplace();
        pwmCreator.Emplace(std::chrono::nanoseconds{ 500 }, hal::Hertz{ 10000 });
        StartAdc(PlatformFactory::SampleAndHold::shortest);
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigurePwm(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 2)
            return { error, "invalid number of arguments" };

        auto deadTime = ParseInput<uint32_t>(tokenizer.Token(0), 500.0f, 2000.0f);
        if (!deadTime.has_value())
            return { error, "invalid value. It should be a float between 500 and 2000." };

        auto frequency = ParseInput<uint32_t>(tokenizer.Token(1), 10000.0f, 20000.0f);
        if (!frequency.has_value())
            return { error, "invalid value. It should be a float between 10000 and 20000." };

        pwmCreator.Destroy();
        pwmCreator.Emplace(std::chrono::nanoseconds{ *deadTime }, hal::Hertz{ *frequency });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigureAdc(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 1)
            return { error, "invalid number of arguments" };

        auto sampleAndHold = ParseInput(tokenizer.Token(0), acceptedAdcValues);
        if (!sampleAndHold)
            return { error, "invalid value. It should be one of: shortest, shorter, medium, longer, longest." };

        StartAdc(ToSampleAndHold(*sampleAndHold));

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigurePid(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 6)
            return { error, "invalid number of arguments" };

        auto dKp = ParseInput<float>(tokenizer.Token(0), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKp.has_value())
            return { error, "invalid value for Speed Kp" };

        auto dKi = ParseInput<float>(tokenizer.Token(1), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKi.has_value())
            return { error, "invalid value for Speed Ki" };

        auto dKd = ParseInput<float>(tokenizer.Token(2), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKd.has_value())
            return { error, "invalid value for Speed Kd" };

        auto qKp = ParseInput<float>(tokenizer.Token(3), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKp.has_value())
            return { error, "invalid value for DQ-axis Kp" };

        auto qKi = ParseInput<float>(tokenizer.Token(4), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKi.has_value())
            return { error, "invalid value for DQ-axis Ki" };

        auto qKd = ParseInput<float>(tokenizer.Token(5), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKd.has_value())
            return { error, "invalid value for DQ-axis Kd" };

        speedPidTunings = controllers::PidTunings<float>{ *dKp, *dKi, *dKd };
        dqPidTunings = controllers::PidTunings<float>{ *qKp, *qKi, *qKd };

        foc.SetSpeedTunings(Vdc, speedPidTunings);
        foc.SetCurrentTunings(Vdc, { dqPidTunings, dqPidTunings });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ReadEncoder()
    {
        tracer.Trace() << "  Encoder Readings:";
        tracer.Trace() << "    Position:  " << encoderCreator->Read().Value() << " radians";

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SimulateFoc(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 4)
            return { error, "invalid number of arguments" };

        auto angle = ParseInput<float>(tokenizer.Token(0), -360.0f, 360.0f);
        if (!angle.has_value())
            return { error, "invalid value for angle. It should be a float between -360 and 360." };

        auto currentA = ParseInput<float>(tokenizer.Token(1), -1000.0f, 1000.0f);
        if (!currentA.has_value())
            return { error, "invalid value for phase A current. It should be a float between -1000 and 1000." };

        auto currentB = ParseInput<float>(tokenizer.Token(2), -1000.0f, 1000.0f);
        if (!currentB.has_value())
            return { error, "invalid value for phase B current. It should be a float between -1000 and 1000." };

        auto currentC = ParseInput<float>(tokenizer.Token(3), -1000.0f, 1000.0f);
        if (!currentC.has_value())
            return { error, "invalid value for phase C current. It should be a float between -1000 and 1000." };

        RunFocSimulation(foc::PhaseCurrents{ foc::Ampere{ *currentA }, foc::Ampere{ *currentB }, foc::Ampere{ *currentC } }, foc::Radians{ *angle * pi_div_180 });

        return { success };
    }

    void TerminalInteractor::RunFocSimulation(foc::PhaseCurrents input, foc::Radians angle)
    {
        performanceTimer.Start();
        auto result = foc.Calculate(input, angle);
        auto duration = performanceTimer.ElapsedCycles();

        tracer.Trace() << "  FOC Simulation Results:";
        tracer.Trace() << "    Vdc:              " << Vdc.Value() << " V";
        tracer.Trace() << "    Pole Pairs:       " << polePairs.value_or(0);
        tracer.Trace() << "    Inputs:";
        tracer.Trace() << "      Angle:            " << angle.Value() << " degrees";
        tracer.Trace() << "      Phase A Current:  " << input.a.Value() << " mA";
        tracer.Trace() << "      Phase B Current:  " << input.b.Value() << " mA";
        tracer.Trace() << "      Phase C Current:  " << input.c.Value() << " mA";
        tracer.Trace() << "    PID Tunings:";
        tracer.Trace() << "      Speed PID:         [P: " << speedPidTunings.kp << ", I: " << speedPidTunings.ki << ", D: " << speedPidTunings.kd << "]";
        tracer.Trace() << "      DQ-axis PID:       [P: " << dqPidTunings.kp << ", I: " << dqPidTunings.ki << ", D: " << dqPidTunings.kd << "]";
        tracer.Trace() << "    PWM Outputs:";
        tracer.Trace() << "      Phase A PWM:      " << result.a.Value() << " %";
        tracer.Trace() << "      Phase B PWM:      " << result.b.Value() << " %";
        tracer.Trace() << "      Phase C PWM:      " << result.c.Value() << " %";
        tracer.Trace() << "    Performance:";
        tracer.Trace() << "      Execution time:   " << duration << " cycles";
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::Stop()
    {
        pwmCreator->Stop();
        return { success };
    }

    void TerminalInteractor::ProcessAdcSamples()
    {
        adcCreator->Stop();
        adcCreator.Destroy();

        tracer.Trace() << "  Current Phases [A;B;C] ampere";

        for (const auto& phase : queueOfPhaseCurrents)
            tracer.Trace() << phase.a.Value() << ";" << phase.b.Value() << ";" << phase.c.Value();

        queueOfPhaseCurrents.clear();
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SetPwmDuty(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 3)
            return { error, "invalid number of arguments" };

        auto dutyA = ParseInput<uint8_t>(tokenizer.Token(0), 1, 99);
        if (!dutyA.has_value())
            return { error, "invalid value for phase A. It should be a float between 1 and 99." };

        auto dutyB = ParseInput<uint8_t>(tokenizer.Token(1), 1, 99);
        if (!dutyB.has_value())
            return { error, "invalid value for phase B. It should be a float between 1 and 99." };

        auto dutyC = ParseInput<uint8_t>(tokenizer.Token(2), 1, 99);
        if (!dutyC.has_value())
            return { error, "invalid value for phase C. It should be a float between 1 and 99." };

        pwmCreator->Start(hal::Percent{ *dutyA }, hal::Percent{ *dutyB }, hal::Percent{ *dutyC });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SetMotorParameters(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 1)
            return { error, "invalid number of arguments" };

        auto poles = ParseInput<uint8_t>(tokenizer.Token(0), 2, 16);
        if (!poles.has_value())
            return { error, "invalid value for poles. It should be an integer between 2 and 16." };

        polePairs = static_cast<std::size_t>(*poles / 2);
        foc.SetPolePairs(polePairs.value());

        return { success };
    }

    void TerminalInteractor::StartAdc(PlatformFactory::SampleAndHold sampleAndHold)
    {
        adcCreator.Emplace(sampleAndHold);
        adcCreator->Measure([this](auto phaseA, auto phaseB, auto phaseC)
            {
                if (!queueOfPhaseCurrents.full())
                    queueOfPhaseCurrents.emplace_back(foc::PhaseCurrents{ phaseA, phaseB, phaseC });
                else
                    ProcessAdcSamples();
            });
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanStart(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 1 || tokenizer.Size() > 2)
            return { error, "invalid number of arguments" };

        auto bitRate = ParseInput<uint32_t>(tokenizer.Token(0), 125000, 1000000);
        if (!bitRate.has_value())
            return { error, "invalid bitrate. It should be between 125000 and 1000000." };

        bool testMode = false;
        if (tokenizer.Size() == 2)
        {
            if (tokenizer.Token(1) == "test")
                testMode = true;
            else
                return { error, "invalid option. Use 'test' for loopback mode." };
        }

        canCreator.Destroy();
        canCreator.Emplace(*bitRate, testMode);
        canStarted = true;

        canCreator->SetOnError([this](CanBusAdapter::CanError error)
            {
                tracer.Trace() << "  CAN Error: " << error;
            });

        tracer.Trace() << "  CAN started at " << *bitRate << " bps" << (testMode ? " (loopback)" : "");

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanStop()
    {
        canCreator.Destroy();
        canStarted = false;
        tracer.Trace() << "  CAN stopped";
        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanSend(const infra::BoundedConstString& param)
    {
        if (!canStarted)
            return { error, "CAN not started. Run 'can_start' first." };

        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 2 || tokenizer.Size() > 9)
            return { error, "invalid number of arguments. Usage: can_send <id> <b0> ... <b7>" };

        auto id = ParseInput<uint32_t>(tokenizer.Token(0), 0, 0x1FFFFFFF);
        if (!id.has_value())
            return { error, "invalid CAN ID. It should be between 0 and 0x1FFFFFFF." };

        hal::Can::Message message;
        for (std::size_t i = 1; i < tokenizer.Size(); ++i)
        {
            auto byte = ParseInput<uint32_t>(tokenizer.Token(i), 0, 255);
            if (!byte.has_value())
                return { error, "invalid data byte. It should be between 0 and 255." };
            message.push_back(static_cast<uint8_t>(*byte));
        }

        hal::Can::Id canId = *id > 0x7FF ? hal::Can::Id::Create29BitId(*id) : hal::Can::Id::Create11BitId(*id);

        canCreator->SendData(canId, message, [this](bool success)
            {
                if (success)
                    tracer.Trace() << "  CAN frame sent";
                else
                    tracer.Trace() << "  CAN frame send failed";
            });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanListen()
    {
        if (!canStarted)
            return { error, "CAN not started. Run 'can_start' first." };

        canCreator->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                uint32_t idValue = id.Is29BitId() ? id.Get29BitId() : id.Get11BitId();
                tracer.Trace() << "  CAN RX [" << (id.Is29BitId() ? "29b" : "11b") << " ID:" << idValue << "] data:";
                for (std::size_t i = 0; i < data.size(); ++i)
                    tracer.Trace() << "    [" << i << "] = " << data[i];
            });

        tracer.Trace() << "  CAN listening for messages";
        return { success };
    }

    void TerminalInteractor::EepromWrite(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 2)
        {
            terminal.ProcessResult({ error, "usage: eeprom_write <addr> <b0> [b1...]" });
            return;
        }

        auto addr = ParseInput<uint32_t>(tokenizer.Token(0), 0u, 65535u);
        if (!addr.has_value())
        {
            terminal.ProcessResult({ error, "invalid address" });
            return;
        }

        const std::size_t byteCount = tokenizer.Size() - 1;
        if (byteCount > eepromBuffer.size())
        {
            terminal.ProcessResult({ error, "too many bytes" });
            return;
        }

        for (std::size_t i = 0; i < byteCount; ++i)
        {
            auto byte = ParseInput<uint32_t>(tokenizer.Token(i + 1), 0.0f, 255.0f);
            if (!byte.has_value())
            {
                terminal.ProcessResult({ error, "invalid byte value" });
                return;
            }
            eepromBuffer[i] = static_cast<uint8_t>(*byte);
        }

        const std::size_t eepromSize = eeprom.Size();
        if (*addr > eepromSize || byteCount > eepromSize - *addr)
        {
            terminal.ProcessResult({ error, "address out of range" });
            return;
        }

        eeprom.WriteBuffer(infra::ConstByteRange{ eepromBuffer.data(), eepromBuffer.data() + byteCount }, *addr, [this]()
            {
                this->terminal.ProcessResult({ success });
            });
    }

    void TerminalInteractor::EepromRead(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 2)
        {
            terminal.ProcessResult({ error, "usage: eeprom_read <addr> <size>" });
            return;
        }

        auto addr = ParseInput<uint32_t>(tokenizer.Token(0), 0u, 65535u);
        if (!addr.has_value())
        {
            terminal.ProcessResult({ error, "invalid address" });
            return;
        }

        auto size = ParseInput<uint32_t>(tokenizer.Token(1), 1u, static_cast<uint32_t>(eepromBuffer.size()));
        if (!size.has_value())
        {
            terminal.ProcessResult({ error, "invalid size" });
            return;
        }

        const std::size_t eepromSize = eeprom.Size();
        if (*addr > eepromSize || *size > eepromSize - *addr)
        {
            terminal.ProcessResult({ error, "address out of range" });
            return;
        }

        eepromCurrentReadSize = *size;
        eeprom.ReadBuffer(infra::ByteRange{ eepromBuffer.data(), eepromBuffer.data() + eepromCurrentReadSize }, *addr, [this]()
            {
                for (uint32_t i = 0; i < this->eepromCurrentReadSize; ++i)
                    this->tracer.Trace() << "  [" << i << "] = " << static_cast<uint32_t>(this->eepromBuffer[i]);
                this->terminal.ProcessResult({ success });
            });
    }

    void TerminalInteractor::EepromErase()
    {
        eeprom.Erase([this]()
            {
                this->terminal.ProcessResult({ success });
            });
    }
}
