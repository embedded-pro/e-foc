#include "targets/hardware_test/components/Terminal.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "foc/interfaces/Driver.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <optional>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float pi_div_180 = std::numbers::pi_v<float> / 180.0f;

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

    std::optional<services::WindingConfiguration> ParseWinding(const infra::BoundedConstString& value)
    {
        if (value == "wye")
            return services::WindingConfiguration::Wye;
        if (value == "delta")
            return services::WindingConfiguration::Delta;
        return std::nullopt;
    }
}

namespace application
{
    using enum services::TerminalWithStorage::Status;

    TerminalInteractor::TerminalInteractor(services::TerminalWithStorage& terminal, application::PlatformFactory& hardware)
        : terminal{ terminal }
        , tracer{ hardware.Tracer() }
        , hardware{ hardware }
        , performanceTimer{ hardware.PerformanceTimer() }
        , Vdc{ hardware.PowerSupplyVoltage() }
        , systemClock{ hardware.SystemClock() }
        , foc{ hardware.MaxCurrentSupported(), baseFrequency_, hardware.LowPriorityInterrupt() }
        , onlineMechEstimator{ services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, foc.OuterLoopFrequency() }
        , onlineElecEstimator{ services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, foc.OuterLoopFrequency() }
        , eeprom{ hardware.Eeprom() }
        , electricalIdent{ hardware, hardware, Vdc }
        , motorAlignment{ hardware, hardware }
    {
        AddCommand({ "enc", "e", "Read encoder. stop. Ex: enc" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(ReadEncoder());
            },
            true);

        AddCommand({ "stop", "stp", "Stop pwm. stop. Ex: stop" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(Stop());
            },
            true);

        AddCommand({ "duty", "d", "Set and start pwm duty. Ex: duty 0 10 25" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(SetPwmDuty(param));
            });

        AddCommand({ "pwm", "p", "Configure pwm [dead_time ns [500; 2000]] [frequency Hz [10000; 20000]]. Ex: pwm 500 10000" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigurePwm(param));
            });

        AddCommand({ "adc", "a", "Configure adc and prints raw data for all three channels [sample_and_hold [short, medium, long]]. Ex: adc short" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigureAdc(param));
            });

        AddCommand({ "pid", "c", "Configure speed and DQ PIDs [spd_kp spd_ki spd_kd dq_kp dq_ki dq_kd]. Ex: pid 1 0 0 1 0 0" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(ConfigurePid(param));
            });

        AddCommand({ "foc", "f", "Simulate foc [pole_pairs angle ia ib ic]. Ex: foc 7 30 1 2 3" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(SimulateFoc(param));
            });

        AddCommand({ "ident", "id", "Identify R, L and pole pairs. ident <wye|delta> [inj_freq_hz] [inj_v%] [pp_v%] [pp_revs] [pp_settle_ms]. Ex: ident wye 250 15 10 5 50" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(IdentifyElectricalParameters(param));
            });

        AddCommand({ "align", "al", "Align rotor using identified pole pairs. align [v%] [samp_hz] [max_samp] [thresh_rad] [count]. Ex: align 20 1000 500 0.001 10" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(AlignMotor(param));
            });

        AddCommand({ "speed", "s", "Run closed-loop speed FOC (requires prior ident). speed <rpm> <kt> [bandwidth_rad_s]. Ex: speed 300 0.05 150" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(RunSpeedFoc(param));
            });

        AddCommand({ "speedstat", "ss", "Report live online estimates (J, b, R, L). Ex: speedstat" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(ReportSpeedEstimates());
            },
            true);

        AddCommand({ "can_start", "cs", "Start CAN bus [bitrate [100000;1000000]] [test]. Ex: can_start 500000" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(CanStart(param));
            });

        AddCommand({ "can_stop", "cx", "Stop CAN bus. Ex: can_stop" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(CanStop());
            });

        AddCommand({ "can_send", "ct", "Send CAN frame [id] [b0] ... [b7]. Ex: can_send 256 1 2 3" },
            [this](const infra::BoundedConstString& param)
            {
                this->terminal.ProcessResult(CanSend(param));
            });

        AddCommand({ "can_listen", "cl", "Listen for CAN messages. Ex: can_listen" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(CanListen());
            });

        AddCommand({ "eeprom_write", "ew", "Write bytes to EEPROM. eeprom_write <addr> <b0> [b1...]. Ex: eeprom_write 0 255 170" },
            [this](const infra::BoundedConstString& param)
            {
                EepromWrite(param);
            });

        AddCommand({ "eeprom_read", "er", "Read bytes from EEPROM. eeprom_read <addr> <size>. Ex: eeprom_read 0 8" },
            [this](const infra::BoundedConstString& param)
            {
                EepromRead(param);
            });

        AddCommand({ "eeprom_erase", "ee", "Erase entire EEPROM. Ex: eeprom_erase" },
            [this](const auto&)
            {
                EepromErase();
            });

        AddCommand({ "reset", "rst", "Reset the device. Ex: reset" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(ResetDevice());
            });

        AddCommand({ "reset_cause", "rc", "Display reset cause. Ex: reset_cause" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(GetResetCauseStatus());
            },
            true);

        AddCommand({ "fault_status", "fs", "Display fault data from previous session. Ex: fault_status" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(GetFaultStatus());
            },
            true);

        AddCommand({ "force_hardfault", "fhf", "Trigger a HardFault exception for error handler validation. Ex: force_hardfault" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(ForceHardfault());
            });

        hardware.SetEncoderResolution(4000);
        hardware.ConfigureAdcAndPwm(hal::Hertz{ 10000 }, std::chrono::nanoseconds{ 500 }, PlatformFactory::SampleAndHold::shortest);
        StartAdc(PlatformFactory::SampleAndHold::shortest);
    }

    void TerminalInteractor::AddCommand(const CommandInfo& info, const CommandHandler& handler, bool allowedWhileSpinning)
    {
        const std::size_t index = guardedCommands.size();
        guardedCommands.emplace_back(GuardedCommand{ handler, allowedWhileSpinning });

        terminal.AddCommand({ info,
            [this, index](const infra::BoundedConstString& params)
            {
                const auto& command = guardedCommands[index];
                if (speedActive_ && !command.allowedWhileSpinning)
                    terminal.ProcessResult({ error, "motor spinning. Run 'stop' first." });
                else
                    command.handler(params);
            } });
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

        currentPwmDeadTime_ = std::chrono::nanoseconds{ *deadTime };
        currentPwmFrequency_ = hal::Hertz{ *frequency };
        adcActive_ = false;
        hardware.ConfigureAdcAndPwm(currentPwmFrequency_, currentPwmDeadTime_, currentSah_);
        StartAdc(currentSah_);

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

        adcActive_ = false;
        hardware.ConfigureAdcAndPwm(currentPwmFrequency_, currentPwmDeadTime_, ToSampleAndHold(*sampleAndHold));
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
        tracer.Trace() << "    Position:  " << hardware.Read().Value() << " radians";

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::SimulateFoc(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 5)
            return { error, "invalid number of arguments" };

        auto pp = ParseInput<uint8_t>(tokenizer.Token(0), 1, 8);
        if (!pp.has_value())
            return { error, "invalid value for pole pairs. It should be an integer between 1 and 8." };

        auto angle = ParseInput<float>(tokenizer.Token(1), -360.0f, 360.0f);
        if (!angle.has_value())
            return { error, "invalid value for angle. It should be a float between -360 and 360." };

        auto currentA = ParseInput<float>(tokenizer.Token(2), -1000.0f, 1000.0f);
        if (!currentA.has_value())
            return { error, "invalid value for phase A current. It should be a float between -1000 and 1000." };

        auto currentB = ParseInput<float>(tokenizer.Token(3), -1000.0f, 1000.0f);
        if (!currentB.has_value())
            return { error, "invalid value for phase B current. It should be a float between -1000 and 1000." };

        auto currentC = ParseInput<float>(tokenizer.Token(4), -1000.0f, 1000.0f);
        if (!currentC.has_value())
            return { error, "invalid value for phase C current. It should be a float between -1000 and 1000." };

        polePairs = static_cast<std::size_t>(*pp);
        foc.SetPolePairs(polePairs.value());
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
        hardware.Stop();

        if (speedActive_)
        {
            foc.Disable();
            speedActive_ = false;
        }

        return { success };
    }

    void TerminalInteractor::ProcessAdcSamples()
    {
        adcActive_ = false;
        hardware.Stop();

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

        hardware.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{ hal::Percent{ *dutyA }, hal::Percent{ *dutyB }, hal::Percent{ *dutyC } });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::IdentifyElectricalParameters(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 1 || tokenizer.Size() > 6)
            return { error, "invalid number of arguments" };

        auto winding = ParseWinding(tokenizer.Token(0));
        if (!winding.has_value())
            return { error, "invalid winding. Use wye or delta." };

        services::ElectricalParametersIdentification::ResistanceAndInductanceConfig rlConfig;
        rlConfig.windingConfig = *winding;

        if (tokenizer.Size() >= 2)
        {
            auto injectionFrequency = ParseInput<uint32_t>(tokenizer.Token(1), 1u, 5000u);
            if (!injectionFrequency.has_value())
                return { error, "invalid value for injection frequency. It should be an integer between 1 and 5000 Hz." };
            rlConfig.injectionFrequency = hal::Hertz{ *injectionFrequency };
        }

        if (tokenizer.Size() >= 3)
        {
            auto injectionVoltage = ParseInput<uint8_t>(tokenizer.Token(2), 1, 100);
            if (!injectionVoltage.has_value())
                return { error, "invalid value for injection voltage. It should be an integer between 1 and 100." };
            rlConfig.injectionVoltagePercent = hal::Percent{ *injectionVoltage };
        }

        pendingPolePairsConfig = {};

        if (tokenizer.Size() >= 4)
        {
            auto ppVoltage = ParseInput<uint8_t>(tokenizer.Token(3), 1, 100);
            if (!ppVoltage.has_value())
                return { error, "invalid value for pole-pairs test voltage. It should be an integer between 1 and 100." };
            pendingPolePairsConfig.testVoltagePercent = hal::Percent{ *ppVoltage };
        }

        if (tokenizer.Size() >= 5)
        {
            auto ppRevs = ParseInput<uint32_t>(tokenizer.Token(4), 1u, 50u);
            if (!ppRevs.has_value())
                return { error, "invalid value for electrical revolutions. It should be an integer between 1 and 50." };
            pendingPolePairsConfig.electricalRevolutions = static_cast<std::size_t>(*ppRevs);
        }

        if (tokenizer.Size() >= 6)
        {
            auto ppSettle = ParseInput<uint32_t>(tokenizer.Token(5), 1u, 10000u);
            if (!ppSettle.has_value())
                return { error, "invalid value for pole-pairs step settle time. It should be an integer between 1 and 10000 ms." };
            pendingPolePairsConfig.settleTimeBetweenSteps = std::chrono::milliseconds{ *ppSettle };
        }

        identificationResults.reset();
        motorAligned = false;

        electricalIdent.EstimateResistanceAndInductance(rlConfig, [this](std::optional<services::ElectricalParametersIdentification::ResistanceInductanceResult> result)
            {
                if (!result.has_value())
                {
                    tracer.Trace() << "  Identification failed: could not estimate R and L.";
                    return;
                }

                identificationResults = IdentificationResults{ *result, 0 };
                RunPolePairEstimation();
            });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::AlignMotor(const infra::BoundedConstString& param)
    {
        if (!identificationResults.has_value() || identificationResults->polePairs == 0)
            return { error, "no pole pairs identified. Run 'ident' first." };

        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() > 5)
            return { error, "invalid number of arguments" };

        services::MotorAlignment::AlignmentConfig config;

        if (tokenizer.Size() >= 1)
        {
            auto voltage = ParseInput<uint8_t>(tokenizer.Token(0), 1, 100);
            if (!voltage.has_value())
                return { error, "invalid value for test voltage. It should be an integer between 1 and 100." };
            config.testVoltagePercent = hal::Percent{ *voltage };
        }

        if (tokenizer.Size() >= 2)
        {
            auto samplingHz = ParseInput<uint32_t>(tokenizer.Token(1), 100u, 20000u);
            if (!samplingHz.has_value())
                return { error, "invalid value for sampling frequency. It should be between 100 and 20000 Hz." };
            config.samplingFrequency = hal::Hertz{ *samplingHz };
        }

        if (tokenizer.Size() >= 3)
        {
            auto maxSamples = ParseInput<uint32_t>(tokenizer.Token(2), 1u, 5000u);
            if (!maxSamples.has_value())
                return { error, "invalid value for max samples. It should be between 1 and 5000." };
            config.maxSamples = static_cast<std::size_t>(*maxSamples);
        }

        if (tokenizer.Size() >= 4)
        {
            auto threshold = ParseInput<float>(tokenizer.Token(3), 0.0001f, 1.0f);
            if (!threshold.has_value())
                return { error, "invalid value for settled threshold. It should be between 0.0001 and 1.0 radians." };
            config.settledThreshold = foc::Radians{ *threshold };
        }

        if (tokenizer.Size() >= 5)
        {
            auto count = ParseInput<uint32_t>(tokenizer.Token(4), 1u, 1000u);
            if (!count.has_value())
                return { error, "invalid value for settled count. It should be between 1 and 1000." };
            config.settledCount = static_cast<std::size_t>(*count);
        }

        motorAlignment.ForceAlignment(identificationResults->polePairs, config, [this](std::optional<foc::Radians> offset)
            {
                if (!offset.has_value())
                {
                    motorAligned = false;
                    tracer.Trace() << "  Alignment failed: rotor did not converge.";
                    return;
                }

                // Rotor is held at the d-axis (electrical angle 0), so zero the encoder here to lock the FOC frame.
                hardware.SetZero();
                motorAligned = true;
                tracer.Trace() << "  Alignment complete. Offset: " << offset->Value() << " radians.";
            });

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::RunSpeedFoc(const infra::BoundedConstString& param)
    {
        if (!identificationResults.has_value() || identificationResults->polePairs == 0)
            return { error, "no pole pairs identified. Run 'ident' first." };

        if (!motorAligned)
            return { error, "motor not aligned. Run 'align' first." };

        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 2 || tokenizer.Size() > 3)
            return { error, "invalid number of arguments" };

        auto rpm = ParseInput<int32_t>(tokenizer.Token(0), -20000, 20000);
        if (!rpm.has_value())
            return { error, "invalid value for target speed. It should be an integer between -20000 and 20000 RPM." };

        auto kt = ParseInput<float>(tokenizer.Token(1), 0.001f, 10.0f);
        if (!kt.has_value())
            return { error, "invalid value for torque constant. It should be a float between 0.001 and 10." };

        float bandwidth = defaultSpeedBandwidthRadPerSec;
        if (tokenizer.Size() == 3)
        {
            auto parsedBandwidth = ParseInput<int32_t>(tokenizer.Token(2), 1, 10000);
            if (!parsedBandwidth.has_value())
                return { error, "invalid value for bandwidth. It should be an integer between 1 and 10000 rad/s." };
            bandwidth = static_cast<float>(*parsedBandwidth);
        }

        const foc::NewtonMeterSecondSquared defaultInertia{ defaultInertiaValue };
        const foc::NewtonMeterSecondPerRadian defaultFriction{ defaultFrictionValue };
        const auto controlFrequency = baseFrequency_;

        foc.SetPolePairs(identificationResults->polePairs);
        foc::WithAutomaticCurrentPidGains{ foc }.SetPidBasedOnResistanceAndInductance(Vdc, identificationResults->rl.resistance, identificationResults->rl.inductance, controlFrequency, currentLoopNyquistFactor);
        foc::WithAutomaticSpeedPidGains{ foc }.SetPidBasedOnInertiaAndFriction(Vdc, defaultInertia, defaultFriction, bandwidth);

        onlineElecEstimator.SetInitialEstimate(identificationResults->rl.resistance, identificationResults->rl.inductance);
        onlineMechEstimator.SetTorqueConstant(foc::NewtonMeter{ *kt });
        onlineMechEstimator.SetInitialEstimate(defaultInertia, defaultFriction);
        foc.SetOnlineMechanicalEstimator(onlineMechEstimator);
        foc.SetOnlineElectricalEstimator(onlineElecEstimator);

        foc.SetPoint(foc::RadiansPerSecond{ static_cast<float>(*rpm) * (2.0f * std::numbers::pi_v<float>) / 60.0f });

        hardware.Stop();
        adcActive_ = false;
        hardware.ConfigureAdcAndPwm(controlFrequency, currentPwmDeadTime_, currentSah_);
        hardware.PhaseCurrentsReady(controlFrequency, [this](foc::PhaseCurrents currentPhases)
            {
                auto position = hardware.Read();
                hardware.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));
            });
        foc.Enable();
        hardware.Start();
        speedActive_ = true;

        tracer.Trace() << "  Speed FOC running at " << *rpm << " RPM";

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ReportSpeedEstimates()
    {
        tracer.Trace() << "  Online Estimates:";
        tracer.Trace() << "    Inertia:      " << onlineMechEstimator.CurrentInertia().Value() << " kg*m^2";
        tracer.Trace() << "    Friction:     " << onlineMechEstimator.CurrentFriction().Value() << " N*m*s/rad";
        tracer.Trace() << "    Resistance:   " << onlineElecEstimator.CurrentResistance().Value() << " Ohm";
        tracer.Trace() << "    Inductance:   " << onlineElecEstimator.CurrentInductance().Value() << " mH";

        return { success };
    }

    void TerminalInteractor::RunPolePairEstimation()
    {
        electricalIdent.EstimateNumberOfPolePairs(pendingPolePairsConfig, [this](std::optional<std::size_t> pp)
            {
                if (!pp.has_value())
                {
                    tracer.Trace() << "  Identification failed: could not estimate pole pairs.";
                    identificationResults.reset();
                    return;
                }

                identificationResults->polePairs = *pp;
                ReportIdentificationResults();
            });
    }

    void TerminalInteractor::ReportIdentificationResults()
    {
        tracer.Trace() << "  Identification Results:";
        tracer.Trace() << "    Resistance:         " << identificationResults->rl.resistance.Value() << " Ohm";
        tracer.Trace() << "    Inductance:         " << identificationResults->rl.inductance.Value() << " mH";
        tracer.Trace() << "    Inverter V offset:  " << identificationResults->rl.inverterVoltageOffset.Value() << " V";
        tracer.Trace() << "    Fit quality:        " << identificationResults->rl.fitQuality;
        tracer.Trace() << "    Pole Pairs:         " << identificationResults->polePairs;
    }

    void TerminalInteractor::StartAdc(PlatformFactory::SampleAndHold sampleAndHold)
    {
        currentSah_ = sampleAndHold;
        adcActive_ = true;
        hardware.PhaseCurrentsReady(currentPwmFrequency_, [this](foc::PhaseCurrents phases)
            {
                if (!adcActive_)
                    return;
                if (!queueOfPhaseCurrents.full())
                    queueOfPhaseCurrents.emplace_back(phases);
                else
                    ProcessAdcSamples();
            });
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanStart(const infra::BoundedConstString& param)
    {
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() < 1 || tokenizer.Size() > 2)
            return { error, "invalid number of arguments" };

        auto bitRate = ParseInput<uint32_t>(tokenizer.Token(0), 100000, 1000000);
        if (!bitRate.has_value())
            return { error, "invalid bitrate. It should be between 100000 and 1000000." };

        bool testMode = false;
        if (tokenizer.Size() == 2)
        {
            if (tokenizer.Token(1) == "test")
                testMode = true;
            else
                return { error, "invalid option. Use 'test' for loopback mode." };
        }

        hardware.ConfigureCanBus(*bitRate, testMode);
        canStarted = true;

        hardware.CanBus().SetOnError([this](CanBusAdapter::CanError error)
            {
                tracer.Trace() << "  CAN Error: " << error;
            });

        tracer.Trace() << "  CAN started at " << *bitRate << " bps" << (testMode ? " (loopback)" : "");

        return { success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::CanStop()
    {
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

        hardware.CanBus().SendData(canId, message, [this](bool success)
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

        hardware.CanBus().ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
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
                tracer.Trace() << "  Written to EEPROM";
                this->terminal.ProcessResult({ success });
            });
        tracer.Trace() << "  Writing to EEPROM at address " << *addr << " bytes: " << byteCount << "...";
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
                tracer.Trace() << "  EEPROM erased";
                this->terminal.ProcessResult({ success });
            });
        tracer.Trace() << "  Erasing entire EEPROM...";
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ResetDevice()
    {
        hardware.Reset();
        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::GetResetCauseStatus()
    {
        infra::BoundedString::WithStorage<64> causeString;
        infra::StringOutputStream stream(causeString, infra::noFail);
        stream << hardware.GetResetCause();
        tracer.Trace() << causeString;
        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::GetFaultStatus()
    {
        const infra::BoundedConstString faultData = hardware.FaultStatus();
        if (faultData.empty())
            tracer.Trace() << "No fault data available.";
        else
            tracer.Trace() << faultData;
        return { services::TerminalWithStorage::Status::success };
    }

    TerminalInteractor::StatusWithMessage TerminalInteractor::ForceHardfault()
    {
        tracer.Trace() << "Triggering HardFault...";
        void (*nullFunc)() = nullptr;
        nullFunc(); // NOSONAR — intentional null dereference to trigger HardFault for error handler validation
        return { services::TerminalWithStorage::Status::success };
    }
}
