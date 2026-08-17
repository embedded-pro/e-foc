#include "core/services/cli/TerminalSpeed.hpp"
#include "core/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocSpeedInteractor::TerminalFocSpeedInteractor(services::TerminalWithStorage& terminal, foc::FocSpeed& foc)
        : TerminalFocBaseInteractor(terminal, foc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_speed_bandwidth", "ssbw", "Set speed loop bandwidth in rad/s. set_speed_bandwidth <bandwidth>. Ex: ssbw 188.5" },
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

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto bandwidth = ParseInput(tokenizer.Token(0));
        if (!bandwidth.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = *bandwidth;
        foc.SetSpeedTunings(tunings);
        return StatusWithMessage();
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
