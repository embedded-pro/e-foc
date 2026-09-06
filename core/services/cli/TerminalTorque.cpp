#include "core/services/cli/TerminalTorque.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/cli/TerminalHelper.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    TerminalFocTorqueInteractor::TerminalFocTorqueInteractor(services::TerminalWithStorage& terminal, foc::FocTorque& foc)
        : TerminalFocBaseInteractor(terminal, foc)
        , foc(foc)
    {
        terminal.AddCommand({ { "set_torque", "st", "Set torque. set_torque <torque>. Ex: st 20.0" },
            [this](const auto& params)
            {
                this->Terminal().ProcessResult(SetTorque(params));
            } });
    }

    TerminalFocTorqueInteractor::StatusWithMessage TerminalFocTorqueInteractor::SetTorque(const infra::BoundedConstString& input)
    {
        float torqueValue = 0.0f;
        auto parsed = ParseSingleBoundedArgument(input, -foc::CommandLimits::maxTorqueSetpoint, foc::CommandLimits::maxTorqueSetpoint, torqueValue);

        if (!Succeeded(parsed))
            return parsed;

        foc.SetPoint(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ torqueValue } });
        return StatusWithMessage();
    }
}
