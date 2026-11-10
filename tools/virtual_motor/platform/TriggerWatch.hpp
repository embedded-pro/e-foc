#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <optional>

namespace virtual_motor
{
    class TriggerWatch
    {
    public:
        static std::optional<port::RefusalReason> Check(const port::Trigger& trigger);

        void Arm(const port::Trigger& trigger);
        bool Fires(const port::Sample& sample, uint8_t leds, bool protection);

    private:
        bool Crossed(float value) const;

        port::Trigger trigger;
        bool armed{ false };
        bool watchedBefore{ false };
        float previousValue{ 0.0f };
        uint8_t previousLeds{ 0 };
        bool previousProtection{ false };
    };
}
