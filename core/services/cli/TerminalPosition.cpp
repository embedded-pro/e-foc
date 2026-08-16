#include "core/services/cli/TerminalPosition.hpp"
#include "core/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocPositionInteractor::TerminalFocPositionInteractor(services::TerminalWithStorage& terminal, foc::FocPosition& foc)
        : TerminalFocBaseInteractor(terminal, foc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_speed_bandwidth", "ssbw", "Set speed loop bandwidth in rad/s. set_speed_bandwidth <bandwidth>. Ex: ssbw 188.5" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetSpeedPid(params));
            } });

        terminal.AddCommand({ { "set_position_bandwidth", "spbw", "Set position loop bandwidth in rad/s. set_position_bandwidth <bandwidth>. Ex: spbw 18.8" },
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

    TerminalFocPositionInteractor::StatusWithMessage TerminalFocPositionInteractor::SetPositionPid(const infra::BoundedConstString& input)
    {
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto bandwidth = ParseInput(tokenizer.Token(0));
        if (!bandwidth.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        auto tunings = foc::PositionLoopTunings{};
        tunings.bandwidth = *bandwidth;

        // Retuning redesigns the position law, which can be refused; never report that as applied
        switch (foc.SetPositionTunings(tunings))
        {
            case foc::SelectResult::busy:
                return { services::TerminalWithStorage::Status::error, "rejected: motor is enabled." };
            case foc::SelectResult::ok:
                return StatusWithMessage();
            default:
                return { services::TerminalWithStorage::Status::error, "rejected: no controller for this bandwidth." };
        }
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
        return StatusWithMessage();
    }
}
