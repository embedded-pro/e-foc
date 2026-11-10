#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    template<typename FocImpl>
    class Runner
        : public PhaseCurrentsObservable
    {
    public:
        Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocImpl& foc);
        ~Runner();

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = delete;
        Runner& operator=(Runner&&) = delete;

        void Enable();
        void Disable();

        void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) override;
        void UnregisterPhaseCurrentsObserver() override;

    private:
        void RegisterPhaseCurrents();
        void ReleasePhaseCurrents();
        void OnPhaseCurrents(const PhaseCurrents& currentPhases);

        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        FocImpl& foc;
        infra::HotSwapFunction<void(const PhaseCurrents& currentPhases)> phaseCurrentsObserver;
        volatile bool stopRequested{ false };
    };

    template<typename FocImpl>
    Runner<FocImpl>::Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocImpl& foc)
        : inverter{ inverter }
        , encoder{ encoder }
        , foc{ foc }
    {}

    template<typename FocImpl>
    Runner<FocImpl>::~Runner()
    {
        Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::Enable()
    {
        stopRequested = false;

        ReleasePhaseCurrents();
        foc.Enable();

        if (stopRequested)
        {
            Disable();
            return;
        }

        inverter.Start();

        if (stopRequested)
        {
            Disable();
            return;
        }

        RegisterPhaseCurrents();

        if (stopRequested)
            Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::Disable()
    {
        stopRequested = true;
        ReleasePhaseCurrents();
        inverter.Stop();
        foc.Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer)
    {
        phaseCurrentsObserver.Set(observer);
    }

    template<typename FocImpl>
    void Runner<FocImpl>::UnregisterPhaseCurrentsObserver()
    {
        phaseCurrentsObserver.Clear();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::RegisterPhaseCurrents()
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](auto currentPhases)
            {
                OnPhaseCurrents(currentPhases);
            });
    }

    template<typename FocImpl>
    void Runner<FocImpl>::ReleasePhaseCurrents()
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [](auto) {});
    }

    template<typename FocImpl>
    OPTIMIZE_FOR_SPEED void Runner<FocImpl>::OnPhaseCurrents(const PhaseCurrents& currentPhases)
    {
        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));

        phaseCurrentsObserver(currentPhases);
    }
}
