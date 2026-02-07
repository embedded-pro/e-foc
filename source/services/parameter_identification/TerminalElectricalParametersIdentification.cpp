#include "source/services/parameter_identification/TerminalElectricalParametersIdentification.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/Tokenizer.hpp"
#include "source/services/parameter_identification/ElectricalParametersIdentification.hpp"

namespace
{
    std::optional<services::WindingConfiguration> ParseStringInput(const infra::BoundedConstString& input)
    {
        if (input == "star" || input == "wye")
            return services::WindingConfiguration::Wye;
        else if (input == "delta")
            return services::WindingConfiguration::Delta;
        else
            return std::nullopt;
    }

    std::optional<foc::Ohm> ParseOhmInput(const infra::BoundedConstString& input)
    {
        float value = 0.0f;
        infra::StringInputStream stream(input, infra::softFail);
        stream >> value;

        if (!stream.ErrorPolicy().Failed() && value >= 0.0f)
            return std::make_optional(foc::Ohm{ value });
        else
            return {};
    }
}

namespace services
{
    TerminalElectricalParametersIdentification::TerminalElectricalParametersIdentification(services::TerminalWithStorage& terminal, services::Tracer& tracer, ElectricalParametersIdentification& identification)
        : terminal(terminal)
        , tracer(tracer)
        , identification(identification)
    {
        terminal.AddCommand({ { "estimate_r_and_l", "estrl", "Estimate motor resistance and inductance. estrl <winding type (star|wye|delta)>. Ex: estrl star" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(EstimateResistanceAndInductance(params));
            } });

        terminal.AddCommand({ { "estimate_pole_pairs", "estpp", "Estimate number of pole pairs. estpp <electrical speed (rad/s)> <mechanical speed (rad/s)>. Ex: estpp 100.0 50.0" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(EstimateNumberOfPolePairs(params));
            } });
    }

    TerminalElectricalParametersIdentification::StatusWithMessage TerminalElectricalParametersIdentification::EstimateResistanceAndInductance(const infra::BoundedConstString& param)
    {
        ElectricalParametersIdentification::ResistanceAndInductanceConfig config;
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 1)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments" };

        auto winding = ParseStringInput(tokenizer.Token(0));
        if (!winding.has_value())
            return { services::TerminalWithStorage::Status::error, "invalid value. It should be an 'star', 'wye' or 'delta'." };

        config.windingConfig = *winding;

        identification.EstimateResistanceAndInductance(config, [this](auto resistance, auto inductance)
            {
                if (!resistance.has_value())
                    tracer.Trace() << "Resistance estimation failed.";
                else if (!inductance.has_value())
                    tracer.Trace() << "Inductance estimation failed.";
                else
                    tracer.Trace() << "Estimated Resistance: " << resistance->Value() << " Ohms, Inductance: " << inductance->Value() << " mH";
            });
        return TerminalElectricalParametersIdentification::StatusWithMessage();
    }

    TerminalElectricalParametersIdentification::StatusWithMessage TerminalElectricalParametersIdentification::EstimateNumberOfPolePairs(const infra::BoundedConstString& param)
    {
        ElectricalParametersIdentification::PolePairsConfig config;

        identification.EstimateNumberOfPolePairs(config, [this](auto polePairs)
            {
                if (!polePairs.has_value())
                    tracer.Trace() << "Pole pairs estimation failed.";
                else
                    tracer.Trace() << "Estimated Pole Pairs: " << *polePairs;
            });

        return TerminalElectricalParametersIdentification::StatusWithMessage();
    }
}
