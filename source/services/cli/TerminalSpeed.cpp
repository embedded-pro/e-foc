#include "source/services/cli/TerminalSpeed.hpp"
#include "infra/util/Tokenizer.hpp"
#include "source/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocSpeedInteractor::TerminalFocSpeedInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocSpeed& foc)
        : TerminalFocBaseInteractor(terminal, vdc, foc)
        , vdc(vdc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_speed_pid", "sspid", "Set speed PID parameters. set_speed_pid <kp> <ki> <kd>. Ex: sspid 1.0 0.2 0.01" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetSpeedPid(params));
            } });

        terminal.AddCommand({ { "set_speed", "ss", "Set speed in rad/s. set_speed <speed>. Ex: ss 20.0" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetSpeed(params));
            } });
    }

    TerminalFocSpeedInteractor::StatusWithMessage TerminalFocSpeedInteractor::SetSpeedPid(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 3)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto kp = ParseInput(tokenizer.Token(0));
        if (!kp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };
        auto ki = ParseInput(tokenizer.Token(1));
        if (!ki.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };
        auto kd = ParseInput(tokenizer.Token(2));
        if (!kd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto pid = controllers::PidTunings<float>{ (*kp), (*ki), (*kd) };

        foc.SetSpeedTunings(vdc, pid);
        return TerminalFocSpeedInteractor::StatusWithMessage();
    }

    TerminalFocSpeedInteractor::StatusWithMessage TerminalFocSpeedInteractor::SetSpeed(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto speedValue = ParseInput(tokenizer.Token(0));
        if (!speedValue.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        foc.SetPoint(foc::RadiansPerSecond(*speedValue));
        return TerminalFocSpeedInteractor::StatusWithMessage();
    }
}
