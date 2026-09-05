#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/Function.hpp"

namespace foc
{
    class Runner
    {
    public:
        Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocBase& foc);
        ~Runner();

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = delete;
        Runner& operator=(Runner&&) = delete;

        void Enable();
        void Disable();

        void RegisterOnImplausibleCurrents(const infra::Function<void()>& onImplausible);

    private:
        void RegisterPhaseCurrents();
        void OnPhaseCurrents(const PhaseCurrents& currentPhases);
        bool CurrentsArePlausible(const PhaseCurrents& currentPhases);

        static constexpr uint8_t implausibleSampleLimit{ 8 };
        static constexpr float residualFraction{ 0.25f };
        static constexpr float residualFloor{ 0.5f };

        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocBase& foc;
        infra::Function<void()> onImplausibleCurrents;
        uint8_t implausibleSamples{ 0 };
        volatile bool enabled{ false };
    };
}
