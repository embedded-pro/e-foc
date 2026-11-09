#include "core/supervision/OverTemperature.hpp"
#include "numerical/math/Math.hpp"
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
        , filter{ 1.0f - math::Exp(-std::chrono::duration<float>(evaluationPeriod).count() / std::chrono::duration<float>(filterTimeConstant).count()) }
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
        const auto measured = Filtered(temperature());
        const auto current = settings();

        Derate(measured, current);
        Trip(enabled, measured, current);
    }

    std::optional<foc::Celsius> OverTemperature::Filtered(const std::optional<foc::Celsius>& measured)
    {
        if (!measured.has_value())
        {
            filterSeeded = false;
            return std::nullopt;
        }

        if (!filterSeeded)
        {
            filter.Reset(measured->Value());
            filterSeeded = true;
            return measured;
        }

        return foc::Celsius{ filter.Filter(measured->Value()) };
    }

    void OverTemperature::Derate(const std::optional<foc::Celsius>& measured, const Settings& current)
    {
        const auto headroom = measured.has_value() ? std::clamp((current.limit.Value() - measured->Value()) / current.margin.Value(), 0.0f, 1.0f) : 1.0f;
        const auto derated = boardLimit.Value() * headroom;

        const auto reachedAnEnd = (headroom == 1.0f || headroom == 0.0f) && derated != limit.Value();
        if (!reachedAnEnd && math::Abs(derated - limit.Value()) < reportStep * boardLimit.Value())
            return;

        limit = foc::Ampere{ derated };
        onLimit(limit);
    }

    void OverTemperature::Trip(bool enabled, const std::optional<foc::Celsius>& measured, const Settings& current)
    {
        if (!enabled)
        {
            enabledFor.Reset();
            return;
        }

        const auto settled = enabledFor.Held(true, infra::Now(), tripHoldOff);
        const auto beyondLimit = measured.has_value() && measured->Value() >= current.limit.Value();

        if (enabledFor.ReportOnce(settled && beyondLimit))
            onFault(state_machine::FaultCode::overtemperature);
    }

    OverTemperature::Settings OverTemperatureSettingsFrom(const services::ConfigData& config)
    {
        return { foc::Celsius{ config.limits.temperatureLimit }, foc::Celsius{ config.limits.temperatureMargin } };
    }
}
