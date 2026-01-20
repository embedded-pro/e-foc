#include "source/services/parameter_identification/TerminalMechanicalParametersIdentification.hpp"
#include "infra/stream/StringInputStream.hpp"
#include "infra/util/Tokenizer.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentification.hpp"

namespace
{
    std::optional<float> ParseFloatInput(const infra::BoundedConstString& input)
    {
        float value = 0.0f;
        infra::StringInputStream stream(input, infra::softFail);
        stream >> value;

        if (!stream.ErrorPolicy().Failed())
            return value;
        else
            return std::nullopt;
    }
}

namespace services
{
    TerminalMechanicalParametersIdentification::TerminalMechanicalParametersIdentification(services::TerminalWithStorage& terminal, services::Tracer& tracer, MechanicalParametersIdentification& identification)
        : terminal(terminal)
        , tracer(tracer)
        , identification(identification)
    {
        terminal.AddCommand({ { "estimate_friction", "estfric", "Estimate friction coefficient (B). estfric <target_speed_rpm> <Kt_Nm_per_A>. Ex: estfric 500 0.1" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(EstimateFriction(params));
            } });

        terminal.AddCommand({ { "estimate_inertia", "estinert", "Estimate inertia (J). Requires friction to be estimated first. estinert <torque_step_A> <Kt_Nm_per_A>. Ex: estinert 1.0 0.1" },
            [this](const auto& params)
            {
                this->terminal.ProcessResult(EstimateInertia(params));
            } });
    }

    TerminalMechanicalParametersIdentification::StatusWithMessage TerminalMechanicalParametersIdentification::EstimateFriction(const infra::BoundedConstString& param)
    {
        MechanicalParametersIdentification::FrictionConfig config;
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 2)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments. Usage: estfric <target_speed_rpm> <Kt_Nm_per_A>" };

        auto targetSpeedRpm = ParseFloatInput(tokenizer.Token(0));
        auto torqueConstant = ParseFloatInput(tokenizer.Token(1));

        if (!targetSpeedRpm.has_value() || *targetSpeedRpm <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid target speed. Must be > 0 RPM" };

        if (!torqueConstant.has_value() || *torqueConstant <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid torque constant. Must be > 0 Nm/A" };

        float targetSpeedRadPerSec = *targetSpeedRpm * 2.0f * 3.14159265f / 60.0f;
        config.targetSpeed = foc::RadiansPerSecond{ targetSpeedRadPerSec };
        config.torqueConstant = foc::NewtonMeter{ *torqueConstant };

        identification.EstimateFriction(config, [this](auto damping)
            {
                if (!damping.has_value())
                    tracer.Trace() << "Friction estimation failed.";
                else
                {
                    lastDamping = damping;
                    tracer.Trace() << "Estimated Friction (B): " << damping->Value() << " Nm·s/rad";
                }
            });

        return TerminalMechanicalParametersIdentification::StatusWithMessage();
    }

    TerminalMechanicalParametersIdentification::StatusWithMessage TerminalMechanicalParametersIdentification::EstimateInertia(const infra::BoundedConstString& param)
    {
        if (!lastDamping.has_value())
            return { services::TerminalWithStorage::Status::error, "friction must be estimated first. Run 'estfric' command." };

        MechanicalParametersIdentification::InertiaConfig config;
        infra::Tokenizer tokenizer(param, ' ');

        if (tokenizer.Size() != 2)
            return { services::TerminalWithStorage::Status::error, "invalid number of arguments. Usage: estinert <torque_step_A> <Kt_Nm_per_A>" };

        auto torqueStepCurrent = ParseFloatInput(tokenizer.Token(0));
        auto torqueConstant = ParseFloatInput(tokenizer.Token(1));

        if (!torqueStepCurrent.has_value() || *torqueStepCurrent <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid torque step current. Must be > 0 A" };

        if (!torqueConstant.has_value() || *torqueConstant <= 0.0f)
            return { services::TerminalWithStorage::Status::error, "invalid torque constant. Must be > 0 Nm/A" };

        config.torqueStepCurrent = foc::Ampere{ *torqueStepCurrent };
        config.torqueConstant = foc::NewtonMeter{ *torqueConstant };

        identification.EstimateInertia(config, *lastDamping, [this](auto inertia)
            {
                if (!inertia.has_value())
                    tracer.Trace() << "Inertia estimation failed.";
                else
                    tracer.Trace() << "Estimated Inertia (J): " << inertia->Value() << " kg·m²";
            });

        return TerminalMechanicalParametersIdentification::StatusWithMessage();
    }
}
