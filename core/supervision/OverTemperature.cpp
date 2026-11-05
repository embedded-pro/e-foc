#include "core/supervision/OverTemperature.hpp"
#include <algorithm>
#include <cmath>
#include <variant>

namespace supervision
{
    OverTemperature::OverTemperature(foc::Ampere boardLimit, const infra::Function<void(foc::Ampere)>& onLimit, const infra::Function<void(state_machine::FaultCode)>& onFault)
        : boardLimit{ boardLimit }
        , onLimit{ onLimit }
        , onFault{ onFault }
        , limit{ boardLimit }
    {}

    void OverTemperature::Enable(const infra::Function<Settings()>& settings)
    {
        this->settings = settings;
        evaluationTimer.Start(evaluationPeriod, [this]()
            {
                Evaluate();
            });
    }

    void OverTemperature::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive, const Temperature& temperature)
    {
        this->activeDrive = activeDrive;
        this->temperature = temperature;
    }

    foc::Ampere OverTemperature::Limit() const
    {
        return limit;
    }

    void OverTemperature::Evaluate()
    {
        if (activeDrive == nullptr || temperature == nullptr)
            return;

        const auto enabled = std::holds_alternative<state_machine::Enabled>(activeDrive().CurrentState());
        const auto measured = temperature();
        const auto current = settings();

        Derate(measured, current);
        Trip(enabled, measured, current);
    }

    void OverTemperature::Derate(const std::optional<foc::Celsius>& measured, const Settings& current)
    {
        const auto headroom = measured.has_value() ? std::clamp((current.limit.Value() - measured->Value()) / current.margin.Value(), 0.0f, 1.0f) : 1.0f;
        const auto derated = boardLimit.Value() * headroom;

        const auto reachedAnEnd = (headroom == 1.0f || headroom == 0.0f) && derated != limit.Value();
        if (!reachedAnEnd && std::abs(derated - limit.Value()) < reportStep * boardLimit.Value())
            return;

        limit = foc::Ampere{ derated };
        onLimit(limit);
    }

    void OverTemperature::Trip(bool enabled, const std::optional<foc::Celsius>& measured, const Settings& current)
    {
        if (!enabled)
        {
            enabledSince.reset();
            reported = false;
            return;
        }

        const auto now = infra::Now();
        if (!enabledSince.has_value())
            enabledSince = now;

        if (reported || !measured.has_value() || measured->Value() < current.limit.Value() || now - *enabledSince < tripHoldOff)
            return;

        reported = true;
        onFault(state_machine::FaultCode::overtemperature);
    }

    OverTemperature::Settings OverTemperatureSettingsFrom(const services::ConfigData& config)
    {
        return { foc::Celsius{ config.limits.temperatureLimit }, foc::Celsius{ config.limits.temperatureMargin } };
    }
}
