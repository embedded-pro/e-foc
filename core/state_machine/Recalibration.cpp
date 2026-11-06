#include "core/state_machine/Recalibration.hpp"
#include "core/foc/math/ParameterValidation.hpp"
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
                default:
                    return model.viscousFriction.Value();
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
                default:
                    model.viscousFriction = foc::NewtonMeterSecondPerRadian{ value };
                    break;
            }
        }

        std::array<float, Recalibration::quantities> ReadingsOf(const OnlineEstimates& estimates)
        {
            return { estimates.resistance.Value(), estimates.inductance.Value(), estimates.inertia.Value(), estimates.viscousFriction.Value() };
        }

        uint16_t Counted(uint16_t count, uint8_t added)
        {
            return static_cast<uint16_t>(std::min<uint32_t>(count + added, std::numeric_limits<uint16_t>::max()));
        }
    }

    void Recalibration::Commission(const MotorModel& model)
    {
        if (model == commissioned)
            return;

        commissioned = model;
        inUse = model;
        tracked = {};
    }

    void Recalibration::Observe(const OnlineEstimates& estimates)
    {
        const auto readings = ReadingsOf(estimates);

        for (std::size_t quantity = 0; quantity != quantities; ++quantity)
        {
            auto& entry = tracked[quantity];
            entry.reading = readings[quantity];
            entry.settled = (quantity < 2 ? estimates.electricalSettled : estimates.mechanicalSettled) && foc::IsFinitePositive(entry.reading);
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
        return entry.settled && foc::IsFinitePositive(current) && std::abs(entry.reading - current) > deviationThreshold * current;
    }

    RecalibrationVerdict Recalibration::Judge(std::size_t quantity, RecalibrationTrigger trigger)
    {
        auto& entry = tracked[quantity];
        const auto reference = QuantityOf(commissioned, quantity);

        if (!foc::IsFinitePositive(reference) || !Differs(quantity) || (trigger == RecalibrationTrigger::automatic && entry.differing < dwell))
            return RecalibrationVerdict::unchanged;

        entry.differing = infra::Duration{};
        const auto ratio = entry.reading / reference;
        if (ratio < bands[quantity].minimum || ratio > bands[quantity].maximum)
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
