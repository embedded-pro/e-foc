#include "core/services/cli/TerminalSpeed.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
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
        return ApplySpeedBandwidth(input, foc);
    }

    TerminalFocSpeedInteractor::StatusWithMessage TerminalFocSpeedInteractor::SetSpeed(const infra::BoundedConstString& input)
    {
        float speedValue = 0.0f;
        auto parsed = ParseSingleBoundedArgument(input, -foc::CommandLimits::maxSpeedSetpoint, foc::CommandLimits::maxSpeedSetpoint, speedValue);

        if (!Succeeded(parsed))
            return parsed;

        foc.SetPoint(foc::RadiansPerSecond(speedValue));
        return StatusWithMessage();
    }
}
