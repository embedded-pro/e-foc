#include "source/application/hardware_test/components/Terminal.hpp"
#include "foc/interfaces/Driver.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <optional>

namespace
{
    constexpr float pi_div_180 = std::numbers::pi_v<float> / 180.0f;

    application::HardwareFactory::SampleAndHold ToSampleAndHold(const infra::BoundedConstString& value)
    {
        if (value == "shortest")
            return application::HardwareFactory::SampleAndHold::shortest;
        else if (value == "shorter")
            return application::HardwareFactory::SampleAndHold::shorter;
        else if (value == "medium")
            return application::HardwareFactory::SampleAndHold::medium;
        else if (value == "longer")
            return application::HardwareFactory::SampleAndHold::longer;
        else if (value == "longest")
            return application::HardwareFactory::SampleAndHold::longest;
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

        auto result = std::find_if(acceptedValues.begin(), acceptedValues.end(),
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
    TerminalInteractor::TerminalInteractor(services::TerminalWithStorage& terminal, application::HardwareFactory& hardware)
        : terminal{ terminal }
        , tracer{ hardware.Tracer() }
        , pwmCreator{ hardware.SynchronousThreeChannelsPwmCreator() }
        , adcCreator{ hardware.AdcMultiChannelCreator() }
        , encoderCreator{ hardware.SynchronousQuadratureEncoderCreator() }
        , performanceTimer{ hardware.PerformanceTimer() }
        , Vdc{ hardware.PowerSupplyVoltage() }
        , systemClock{ hardware.SystemClock() }
        , foc{ hardware.MaxCurrentSupported(), std::chrono::microseconds(static_cast<std::size_t>(1e6f / 1000.0f)) }
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

        encoderCreator.Emplace();
        pwmCreator.Emplace(std::chrono::nanoseconds{ 500 }, hal::Hertz{ 10000 });
        StartAdc(HardwareFactory::SampleAndHold::shortest);
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigurePwm(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 2)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto deadTime = ParseInput<uint32_t>(tokenizer.Token(0), 500.0f, 2000.0f);
        if (!deadTime.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float between 500 and 2000." };

        auto frequency = ParseInput<uint32_t>(tokenizer.Token(1), 10000.0f, 20000.0f);
        if (!frequency.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float between 10000 and 20000." };

        pwmCreator.Destroy();
        pwmCreator.Emplace(std::chrono::nanoseconds{ *deadTime }, hal::Hertz{ *frequency });

        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigureAdc(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto sampleAndHold = ParseInput(tokenizer.Token(0), acceptedAdcValues);
        if (!sampleAndHold)
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be one of: shortest, shorter, medium, longer, longest." };

        StartAdc(ToSampleAndHold(*sampleAndHold));

        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ConfigurePid(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 6)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto dKp = ParseInput<float>(tokenizer.Token(0), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for Speed Kp" };

        auto dKi = ParseInput<float>(tokenizer.Token(1), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKi.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for Speed Ki" };

        auto dKd = ParseInput<float>(tokenizer.Token(2), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!dKd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for Speed Kd" };

        auto qKp = ParseInput<float>(tokenizer.Token(3), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for DQ-axis Kp" };

        auto qKi = ParseInput<float>(tokenizer.Token(4), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKi.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for DQ-axis Ki" };

        auto qKd = ParseInput<float>(tokenizer.Token(5), std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
        if (!qKd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for DQ-axis Kd" };

        speedPidTunings = controllers::PidTunings<float>{ *dKp, *dKi, *dKd };
        dqPidTunings = controllers::PidTunings<float>{ *qKp, *qKi, *qKd };

        foc.SetSpeedTunings(Vdc, speedPidTunings);
        foc.SetCurrentTunings(Vdc, { dqPidTunings, dqPidTunings });

        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ReadEncoder()
    {
        tracer.Trace() << "  Encoder Readings:";
        tracer.Trace() << "    Position:  " << encoderCreator->Read().Value() << " radians";

        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SimulateFoc(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 4)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto angle = ParseInput<float>(tokenizer.Token(0), -360.0f, 360.0f);
        if (!angle.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for angle. It should be a float between -360 and 360." };

        auto currentA = ParseInput<float>(tokenizer.Token(1), -1000.0f, 1000.0f);
        if (!currentA.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase A current. It should be a float between -1000 and 1000." };

        auto currentB = ParseInput<float>(tokenizer.Token(2), -1000.0f, 1000.0f);
        if (!currentB.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase B current. It should be a float between -1000 and 1000." };

        auto currentC = ParseInput<float>(tokenizer.Token(3), -1000.0f, 1000.0f);
        if (!currentC.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase C current. It should be a float between -1000 and 1000." };

        RunFocSimulation(foc::PhaseCurrents{ foc::Ampere{ *currentA }, foc::Ampere{ *currentB }, foc::Ampere{ *currentC } }, foc::Radians{ *angle * pi_div_180 });

        return { services::TerminalWithStorage::Status::success };
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
        return { services::TerminalWithStorage::Status::success };
    }

    void TerminalInteractor::ProcessAdcSamples()
    {
        adcCreator->Stop();
        adcCreator.Destroy();

        tracer.Trace() << "  Current Phases [A;B;C] ampere";

        for (std::size_t i = 0; i < queueOfPhaseCurrents.size(); ++i)
            tracer.Trace() << queueOfPhaseCurrents[i].a.Value() << ";" << queueOfPhaseCurrents[i].b.Value() << ";" << queueOfPhaseCurrents[i].c.Value();

        queueOfPhaseCurrents.clear();
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SetPwmDuty(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 3)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto dutyA = ParseInput<uint8_t>(tokenizer.Token(0), 1, 99);
        if (!dutyA.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase A. It should be a float between 1 and 99." };

        auto dutyB = ParseInput<uint8_t>(tokenizer.Token(1), 1, 99);
        if (!dutyB.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase B. It should be a float between 1 and 99." };

        auto dutyC = ParseInput<uint8_t>(tokenizer.Token(2), 1, 99);
        if (!dutyC.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for phase C. It should be a float between 1 and 99." };

        pwmCreator->Start(hal::Percent{ *dutyA }, hal::Percent{ *dutyB }, hal::Percent{ *dutyC });

        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SetMotorParameters(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto poles = ParseInput<uint8_t>(tokenizer.Token(0), 2, 16);
        if (!poles.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value for poles. It should be an integer between 2 and 16." };

        polePairs = static_cast<std::size_t>(*poles / 2);
        foc.SetPolePairs(polePairs.value());

        return { services::TerminalWithStorage::Status::success };
    }

    void TerminalInteractor::StartAdc(HardwareFactory::SampleAndHold sampleAndHold)
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
}
