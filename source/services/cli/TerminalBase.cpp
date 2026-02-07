#include "source/services/cli/TerminalBase.hpp"
#include "infra/util/Tokenizer.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "source/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocBaseInteractor::TerminalFocBaseInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::ControllerBase& foc)
        : terminal(terminal)
        , vdc(vdc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_dq_pid", "sdqpid", "Set D and Q PID parameters. set_dq_pid <kp> <ki> <kd> <kp> <ki> <kd>. Ex: sdqpid 1.0 0.765 -0.56 0.5 -0.35 0.75" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(SetFocPid(params));
            } });

        terminal.AddCommand({ { "start", "sts", "Start system. start. Ex: start" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(Start());
            } });

        terminal.AddCommand({ { "stop", "stp", "Stop system. stop. Ex: stop" },
            [this](const auto&)
            {
                this->terminal.ProcessResult(Stop());
            } });
    }

    services::TerminalWithStorage& TerminalFocBaseInteractor::Terminal()
    {
        return terminal;
    }

    TerminalFocBaseInteractor::StatusWithMessage TerminalFocBaseInteractor::SetFocPid(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 6)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto dkp = ParseInput(tokenizer.Token(0));
        if (!dkp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto dki = ParseInput(tokenizer.Token(1));
        if (!dki.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto dkd = ParseInput(tokenizer.Token(2));
        if (!dkd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto qkp = ParseInput(tokenizer.Token(3));
        if (!qkp.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto qki = ParseInput(tokenizer.Token(4));
        if (!qki.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto qkd = ParseInput(tokenizer.Token(5));
        if (!qkd.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto dPid = controllers::PidTunings<float>{ (*dkp), (*dki), (*dkd) };
        auto qPid = controllers::PidTunings<float>{ (*qkp), (*qki), (*qkd) };

        foc.SetCurrentTunings(vdc, foc::IdAndIqTunings{ dPid, qPid });
        return TerminalFocBaseInteractor::StatusWithMessage();
    }

    TerminalFocBaseInteractor::StatusWithMessage TerminalFocBaseInteractor::Start()
    {
        foc.Enable();
        return TerminalFocBaseInteractor::StatusWithMessage();
    }

    TerminalFocBaseInteractor::StatusWithMessage TerminalFocBaseInteractor::Stop()
    {
        foc.Disable();
        return TerminalFocBaseInteractor::StatusWithMessage();
    }
}
