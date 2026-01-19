#pragma once

#include "foc/instantiations/TrigonometricImpl.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/alignment/TerminalMotorAlignment.hpp"
#include "services/parameter_identification/TerminalMotorIdentification.hpp"
#include "source/foc/implementations/ControllerBaseImpl.hpp"
#include "source/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include "source/services/alignment/MotorAlignmentImpl.hpp"
#include "source/services/cli/TerminalBase.hpp"
#include "source/services/cli/TerminalHelper.hpp"
#include "source/services/parameter_identification/MotorIdentificationImpl.hpp"
#include <optional>
#include <type_traits>
#include <variant>

namespace application
{
    struct TerminalAndTracer
    {
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
    };

    struct MotorDriverAndEncoder
    {
        foc::MotorDriver& driver;
        foc::Encoder& encoder;
    };

    template<typename FocImpl, typename ControllerImpl, typename TerminalImpl>
    constexpr bool IsValidMotorStateMachineTypes_v =
        std::is_base_of_v<foc::FieldOrientedControllerBase, FocImpl> &&
        std::is_base_of_v<foc::ControllerBaseImpl, ControllerImpl> &&
        std::is_base_of_v<services::TerminalFocBaseInteractor, TerminalImpl> &&
        foc::IsWithAutomaticCurrentPidGains_v<ControllerImpl>;

    template<typename FocImpl, typename ControllerImpl, typename TerminalImpl, typename = std::enable_if_t<IsValidMotorStateMachineTypes_v<FocImpl, ControllerImpl, TerminalImpl>>>
    class MotorStateMachine
    {
    public:
        template<typename... FocArgs>
        MotorStateMachine(const TerminalAndTracer& terminalAndTracer, const MotorDriverAndEncoder& motorDriverAndEncoder, foc::Volts vdc, FocArgs&&... focArgs);

    private:
        std::optional<std::pair<foc::Ohm, foc::MilliHenry>> ParseArgs(infra::Tokenizer& tokenizer);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        foc::MotorDriver& driver;
        foc::Encoder& encoder;
        foc::Volts vdc;
        foc::TrigonometricFunctions trigonometricFunctions;
        FocImpl focImpl;
        std::variant<std::monostate, services::MotorAlignmentImpl, services::MotorIdentificationImpl, ControllerImpl> motorStates;
        std::variant<std::monostate, services::TerminalMotorAlignment, services::TerminalMotorIdentification, TerminalImpl> terminalStates;
    };

    // Implementation

    template<typename FocImpl, typename ControllerImpl, typename TerminalImpl, typename Enable>
    template<typename... FocArgs>
    MotorStateMachine<FocImpl, ControllerImpl, TerminalImpl, Enable>::MotorStateMachine(const TerminalAndTracer& terminalAndTracer, const MotorDriverAndEncoder& motorDriverAndEncoder, foc::Volts vdc, FocArgs&&... focArgs)
        : terminal(terminalAndTracer.terminal)
        , tracer(terminalAndTracer.tracer)
        , driver(motorDriverAndEncoder.driver)
        , encoder(motorDriverAndEncoder.encoder)
        , vdc(vdc)
        , focImpl{ trigonometricFunctions, std::forward<FocArgs>(focArgs)... }
    {
        terminal.AddCommand({ { "ident_par", "ip", "Identify Parameters, which are resistance, inductance and number of pole pairs." },
            [this](const auto&)
            {
                motorStates.template emplace<services::MotorIdentificationImpl>(this->driver, this->encoder, this->vdc);
                terminalStates.template emplace<services::TerminalMotorIdentification>(this->terminal, this->tracer, std::get<services::MotorIdentificationImpl>(this->motorStates));
            } });

        terminal.AddCommand({ { "align_motor", "am", "Align Motor." },
            [this](const auto&)
            {
                motorStates.template emplace<services::MotorAlignmentImpl>(this->driver, this->encoder, this->vdc);
                terminalStates.template emplace<services::TerminalMotorAlignment>(this->terminal, this->tracer, std::get<services::MotorAlignmentImpl>(this->motorStates));
            } });

        terminal.AddCommand({ { "foc", "foc", "Start FOC controller with R and L parameters. foc <resistance_ohm> <inductance_mH>. Ex: foc 0.5 1.0" },
            [this](const infra::BoundedConstString& params)
            {
                constexpr float nyquistFactor = 15.0f;
                infra::Tokenizer tokenizer(params, ' ');

                auto args = ParseArgs(tokenizer);

                if (!args.has_value())
                    return;

                this->focImpl.Reset();
                motorStates.template emplace<ControllerImpl>(this->driver, this->encoder, this->focImpl);
                auto& focController = std::get<ControllerImpl>(this->motorStates);
                focController.SetPidBasedOnResistanceAndInductance(this->vdc, std::get<0>(*args), std::get<1>(*args), nyquistFactor);
                terminalStates.template emplace<TerminalImpl>(this->terminal, this->vdc, focController, focController);
            } });
    }

    template<typename FocImpl, typename ControllerImpl, typename TerminalImpl, typename Enable>
    std::optional<std::pair<foc::Ohm, foc::MilliHenry>> MotorStateMachine<FocImpl, ControllerImpl, TerminalImpl, Enable>::ParseArgs(infra::Tokenizer& tokenizer)
    {
        if (tokenizer.Size() != 2)
        {
            this->terminal.ProcessResult({ services::TerminalWithStorage::Status::error, "invalid number of arguments" });
            return std::nullopt;
        }

        auto resistance = services::ParseInput(tokenizer.Token(0));
        if (!resistance.has_value())
        {
            this->terminal.ProcessResult({ services::TerminalWithStorage::Status::error, "invalid resistance value. It should be a float." });
            return std::nullopt;
        }

        auto inductance = services::ParseInput(tokenizer.Token(1));
        if (!inductance.has_value())
        {
            this->terminal.ProcessResult({ services::TerminalWithStorage::Status::error, "invalid inductance value. It should be a float." });
            return std::nullopt;
        }

        return std::make_pair(foc::Ohm{ *resistance }, foc::MilliHenry{ *inductance });
    }
}
