#include "source/services/parameter_identification/TerminalMechanicalParametersIdentification.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/Tokenizer.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"

namespace
{
    template<typename T>
    std::optional<T> ParseNumberInput(const infra::BoundedConstString& input)
    {
        T value = 0.0f;
        infra::StringInputStream stream(input, infra::softFail);
        stream >> value;

        if (!stream.ErrorPolicy().Failed())
            return value;
        else
            return std::nullopt;
    }

    foc::RadiansPerSecond ToRadiansPerSecond(float rpm)
    {
        return foc::RadiansPerSecond{ rpm * 2.0f * std::numbers::pi_v<float> / 60.0f };
    }
}

namespace services
{
    TerminalMechanicalParametersIdentification::TerminalMechanicalParametersIdentification(services::TerminalWithStorage& terminal, services::Tracer& tracer, MechanicalParametersIdentification& identification)
        : terminal(terminal)
        , tracer(tracer)
        , identification(identification)
    {
        terminal.AddCommand({ { "estimate_mechanical", "estmech", "Estimate friction coefficient (B). estmech <target_speed_rpm> <Kt_Nm_per_A> <number_of_pole_pairs>. Ex: estmech 500.0 0.42 7" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(EstimateFrictionAndInertia(params));
            } });
    }

    TerminalMechanicalParametersIdentification::StatusWithMessage TerminalMechanicalParametersIdentification::EstimateFrictionAndInertia(const infra::BoundedConstString& param)
    {
        MechanicalParametersIdentification::Config config;
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 3)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments. Usage: estmech <target_speed_rpm> <Kt_Nm_per_A> <number_of_pole_pairs>" };

        auto targetSpeedRpm = ParseNumberInput<float>(tokenizer.Token(0));
        auto torqueConstant = ParseNumberInput<float>(tokenizer.Token(1));
        auto numberOfPolePairs = ParseNumberInput<uint32_t>(tokenizer.Token(2));

        if (!targetSpeedRpm.has_value() || *targetSpeedRpm <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid target speed. Must be > 0 RPM" };

        if (!torqueConstant.has_value() || *torqueConstant <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid torque constant. Must be > 0 Nm/A" };

        if (!numberOfPolePairs.has_value() || *numberOfPolePairs <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid number of pole pairs. Must be > 0" };

        config.targetSpeed = ToRadiansPerSecond(*targetSpeedRpm);

        identification.EstimateFrictionAndInertia(foc::NewtonMeter{ *torqueConstant }, *numberOfPolePairs, config, [this](auto friction, auto inertia)
            {
                if (!friction.has_value() || !inertia.has_value())
                    tracer.Trace() << "Friction and inertia estimation failed.";
                else
                {
                    tracer.Trace() << "Estimated Friction (B): " << friction->Value() << " Nm·s/rad";
                    tracer.Trace() << "Estimated Inertia (J): " << inertia->Value() << " Nm·s²";
                }
            });

        return TerminalMechanicalParametersIdentification::StatusWithMessage();
    }
}
