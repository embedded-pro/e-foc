#include "source/services/cli/TerminalTorque.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "source/foc/interfaces/Foc.hpp"
#include "source/foc/interfaces/Units.hpp"
#include "source/services/cli/TerminalHelper.hpp"

namespace services
{
    TerminalFocTorqueInteractor::TerminalFocTorqueInteractor(services::TerminalWithStorage& terminal, foc::Volts vdc, foc::FocTorque& foc)
        : TerminalFocBaseInteractor(terminal, vdc, foc)
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
        infra::Tokenizer tokenizer(input, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments." };

        auto t = ParseInput(tokenizer.Token(0));
        if (!t.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be a float." };

        foc::Ampere current(*t);

        foc.SetPoint(foc::IdAndIqPoint{ current, 0.0f });
        return TerminalFocTorqueInteractor::StatusWithMessage();
    }
}
