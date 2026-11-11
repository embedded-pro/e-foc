#include "core/state_machine/Recalibration.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace state_machine
{
    namespace
    {
        float QuantityOf(const MotorModel& model, std::size_t quantity)
        {
            switch (quantity)
            {
                case 0:
                    return model.resistance.Value();
                case 1:
                    return model.inductance.Value();
                case 2:
                    return model.inertia.Value();
                case 3:
                    return model.viscousFriction.Value();
                default:
                    return model.fluxLinkage.Value();
            }
        }

        void Adopt(MotorModel& model, std::size_t quantity, float value)
        {
            switch (quantity)
            {
                case 0:
                    model.resistance = foc::Ohm{ value };
                    break;
                case 1:
                    model.inductance = foc::MilliHenry{ value };
                    break;
                case 2:
                    model.inertia = foc::NewtonMeterSecondSquared{ value };
                    break;
                case 3:
                    model.viscousFriction = foc::NewtonMeterSecondPerRadian{ value };
                    break;
                default:
                    model.fluxLinkage = foc::Weber{ value };
                    break;
            }
        }

        std::array<float, Recalibration::quantities> ReadingsOf(const OnlineEstimates& estimates)
        {
            return { estimates.resistance.Value(), estimates.inductance.Value(), estimates.inertia.Value(), estimates.viscousFriction.Value(), estimates.fluxLinkage.Value() };
        }

        uint16_t Counted(uint16_t count, uint8_t added)
        {
            return static_cast<uint16_t>(std::min<uint32_t>(count + added, std::numeric_limits<uint16_t>::max()));
        }

        template<class Quantity>
        std::optional<float> ValueOf(const std::optional<Quantity>& quantity)
        {
            return quantity.has_value() ? std::optional<float>{ quantity->Value() } : std::nullopt;
        }

        std::array<std::optional<float>, Recalibration::quantities> QuantitiesOf(const StoredModel& stored)
        {
            return { std::nullopt, ValueOf(stored.inductance), ValueOf(stored.inertia), ValueOf(stored.viscousFriction), std::nullopt };
        }

        bool WithinBand(std::size_t quantity, float value, float reference)
        {
            const auto ratio = value / reference;
            return ratio >= Recalibration::bands[quantity].minimum && ratio <= Recalibration::bands[quantity].maximum;
        }

        bool SettledIn(const OnlineEstimates& estimates, std::size_t quantity)
        {
            if (quantity < 2)
                return estimates.electricalSettled;

            return quantity < 4 ? estimates.mechanicalSettled : estimates.fluxLinkageSettled;
        }

        template<class Quantity>
        std::optional<Quantity> IfChanged(Quantity inUse, Quantity commissioned)
        {
            return inUse != commissioned ? std::optional<Quantity>{ inUse } : std::nullopt;
        }
    }

    bool Recalibration::Commission(const MotorModel& model, const StoredModel& stored)
    {
        if (model == commissioned && stored == restored)
            return false;

        commissioned = model;
        restored = stored;
        inUse = model;
        tracked = {};

        const auto values = QuantitiesOf(stored);
        for (std::size_t quantity = 0; quantity != quantities; ++quantity)
        {
            const auto reference = QuantityOf(commissioned, quantity);
            if (values[quantity].has_value() && foc::IsFinitePositive(reference) && foc::IsFinitePositive(*values[quantity]) && WithinBand(quantity, *values[quantity], reference))
                Adopt(inUse, quantity, *values[quantity]);
        }

        return true;
    }

    bool Recalibration::IsCommissionedFrom(const MotorModel& model, const StoredModel& stored) const
    {
        return model == commissioned && stored == restored;
    }

    void Recalibration::Revert()
    {
        restored = {};
        inUse = commissioned;
        tracked = {};
    }

    StoredModel Recalibration::Learned() const
    {
        return StoredModel{ IfChanged(inUse.inductance, commissioned.inductance), IfChanged(inUse.inertia, commissioned.inertia), IfChanged(inUse.viscousFriction, commissioned.viscousFriction) };
    }

    void Recalibration::Observe(const OnlineEstimates& estimates)
    {
        const auto readings = ReadingsOf(estimates);

        for (std::size_t quantity = 0; quantity != quantities; ++quantity)
        {
            auto& entry = tracked[quantity];
            entry.reading = readings[quantity];
            entry.settled = SettledIn(estimates, quantity) && foc::IsFinitePositive(entry.reading);
            entry.differing = Differs(quantity) ? entry.differing + observationPeriod : infra::Duration{};
        }
    }

    Recalibration::Outcome Recalibration::Decide(RecalibrationTrigger trigger)
    {
        Outcome outcome;

        for (std::size_t quantity = 0; quantity != quantities; ++quantity)
        {
            outcome.readings[quantity] = tracked[quantity].reading;
            outcome.verdicts[quantity] = Judge(quantity, trigger);
            outcome.applied += outcome.verdicts[quantity] == RecalibrationVerdict::applied ? 1 : 0;
            outcome.rejected += outcome.verdicts[quantity] == RecalibrationVerdict::rejected ? 1 : 0;
        }

        applied = Counted(applied, outcome.applied);
        rejected = Counted(rejected, outcome.rejected);
        return outcome;
    }

    bool Recalibration::Differs(std::size_t quantity) const
    {
        const auto& entry = tracked[quantity];
        const auto current = QuantityOf(inUse, quantity);
        return entry.settled && foc::IsFinitePositive(current) && math::Abs(entry.reading - current) > deviationThresholds[quantity] * current;
    }

    RecalibrationVerdict Recalibration::Judge(std::size_t quantity, RecalibrationTrigger trigger)
    {
        auto& entry = tracked[quantity];
        const auto reference = QuantityOf(commissioned, quantity);

        if (!foc::IsFinitePositive(reference) || !Differs(quantity) || (trigger == RecalibrationTrigger::automatic && entry.differing < dwell))
            return RecalibrationVerdict::unchanged;

        entry.differing = infra::Duration{};
        if (!WithinBand(quantity, entry.reading, reference))
            return RecalibrationVerdict::rejected;

        Adopt(inUse, quantity, entry.reading);
        return RecalibrationVerdict::applied;
    }

    const MotorModel& Recalibration::Commissioned() const
    {
        return commissioned;
    }

    const MotorModel& Recalibration::InUse() const
    {
        return inUse;
    }

    uint16_t Recalibration::Applied() const
    {
        return applied;
    }

    uint16_t Recalibration::Rejected() const
    {
        return rejected;
    }
}
