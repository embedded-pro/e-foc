#include "tools/virtual_motor/platform/TriggerWatch.hpp"
#include "numerical/math/Math.hpp"

namespace virtual_motor
{
    std::optional<port::RefusalReason> TriggerWatch::Check(const port::Trigger& trigger)
    {
        if (trigger.kind > port::TriggerKind::leds || trigger.signal >= port::signalCount || !math::IsFinite(trigger.level))
            return port::RefusalReason::outOfRange;
        return std::nullopt;
    }

    void TriggerWatch::Arm(const port::Trigger& armedTrigger)
    {
        trigger = armedTrigger;
        armed = true;
        watchedBefore = false;
    }

    bool TriggerWatch::Fires(const port::Sample& sample, uint8_t leds, bool protection)
    {
        if (!armed)
            return false;

        const auto value = sample[trigger.signal];
        bool fired{ false };
        if (trigger.kind == port::TriggerKind::immediately)
            fired = true;
        else if (watchedBefore && trigger.kind == port::TriggerKind::protection)
            fired = protection && !previousProtection;
        else if (watchedBefore && trigger.kind == port::TriggerKind::leds)
            fired = (leds & trigger.leds) != 0 && (previousLeds & trigger.leds) == 0;
        else if (watchedBefore)
            fired = Crossed(value);

        watchedBefore = true;
        previousValue = value;
        previousLeds = leds;
        previousProtection = protection;
        armed = !fired;
        return fired;
    }

    bool TriggerWatch::Crossed(float value) const
    {
        const bool rose = previousValue < trigger.level && value >= trigger.level;
        const bool fell = previousValue > trigger.level && value <= trigger.level;
        switch (trigger.kind)
        {
            case port::TriggerKind::rising:
                return rose;
            case port::TriggerKind::falling:
                return fell;
            default:
                return rose || fell;
        }
    }
}
