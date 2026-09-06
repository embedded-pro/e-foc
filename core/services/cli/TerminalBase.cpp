#include "core/services/cli/TerminalBase.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/services/cli/TerminalHelper.hpp"
#include "infra/util/Tokenizer.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    TerminalFocBaseInteractor::TerminalFocBaseInteractor(services::TerminalWithStorage& terminal, foc::CurrentLoopTunable& currentLoop)
        : terminal(terminal)
        , currentLoop(currentLoop)
    {
        terminal.AddCommand({ { "set_current_bandwidth", "scbw", "Set current loop bandwidth in rad/s. set_current_bandwidth <bandwidth>. Ex: scbw 6283.2" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(SetFocPid(params));
            } });
    }

    services::TerminalWithStorage& TerminalFocBaseInteractor::Terminal()
    {
        return terminal;
    }

    TerminalFocBaseInteractor::StatusWithMessage TerminalFocBaseInteractor::SetFocPid(const infra::BoundedConstString& input)
    {
        float bandwidth = 0.0f;
        auto parsed = ParseSingleBoundedArgument(input, foc::CommandLimits::minBandwidth, foc::CommandLimits::maxCurrentBandwidth, bandwidth);

        if (!Succeeded(parsed))
            return parsed;

        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth;
        currentLoop.SetCurrentTunings(tunings);
        return StatusWithMessage();
    }
}
