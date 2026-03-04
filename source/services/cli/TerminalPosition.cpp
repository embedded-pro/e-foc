#include "source/services/cli/TerminalPosition.hpp"
#include "infra/util/Tokenizer.hpp"
#include "source/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocPositionInteractor::TerminalFocPositionInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocPosition& foc)
        : TerminalFocBaseInteractor(terminal, vdc, foc)
        , vdc(vdc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_speed_pid", "sspid", "Set speed PID parameters. set_speed_pid <kp> <ki> <kd>. Ex: sspid 1.0 0.2 0.01" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetSpeedPid(params));
            } });

        terminal.AddCommand({ { "set_position_pid", "sppid", "Set position PID parameters. set_position_pid <kp> <ki> <kd>. Ex: sppid 5.0 0.1 0.0" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetPositionPid(params));
            } });

        terminal.AddCommand({ { "set_position", "sp", "Set position in radians. set_position <position>. Ex: sp 3.14" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetPosition(params));
            } });
    }

    TerminalFocPositionInteractor::StatusWithMessage TerminalFocPositionInteractor::SetSpeedPid(const infra::BoundedConstString& input)
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
        return TerminalFocPositionInteractor::StatusWithMessage();
    }

    TerminalFocPositionInteractor::StatusWithMessage TerminalFocPositionInteractor::SetPositionPid(const infra::BoundedConstString& input)
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

        foc.SetPositionTunings(pid);
        return TerminalFocPositionInteractor::StatusWithMessage();
    }

    TerminalFocPositionInteractor::StatusWithMessage TerminalFocPositionInteractor::SetPosition(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto positionValue = ParseInput(tokenizer.Token(0));
        if (!positionValue.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        foc.SetPoint(foc::Radians(*positionValue));
        return TerminalFocPositionInteractor::StatusWithMessage();
    }
}
