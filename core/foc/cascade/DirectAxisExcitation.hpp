#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <chrono>
#include <cstdint>

namespace foc
{
    class DirectAxisExcitation
    {
    public:
        DirectAxisExcitation(Ampere amplitude, hal::Hertz frequency, hal::Hertz outerLoopFrequency, std::chrono::seconds burst, std::chrono::seconds cycle);

        void Restart();
        float Advance();

    private:
        float amplitude;
        uint32_t halfPeriodSamples;
        uint32_t burstSamples;
        uint32_t cycleSamples;
        uint32_t sample{ 0 };
        uint32_t cycleSample{ 0 };
        bool positive{ true };
    };
}
