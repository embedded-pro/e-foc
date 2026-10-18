#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>

namespace foc
{
    template<typename FocImpl>
    class Runner
        : public PhaseCurrentsObservable
    {
    public:
        Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocImpl& foc);
        ~Runner() override;

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
        infra::Function<void(const PhaseCurrents& currentPhases)> phaseCurrentsObserver;
        volatile bool observerRegistered{ false };
        volatile bool enabled{ false };
        volatile bool stopRequested{ false };
    };

    template<typename FocImpl>
    Runner<FocImpl>::Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocImpl& foc)
        : inverter{ inverter }
        , encoder{ encoder }
        , foc{ foc }
    {
        RegisterPhaseCurrents();
    }

    template<typename FocImpl>
    Runner<FocImpl>::~Runner()
    {
        Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::Enable()
    {
        stopRequested = false;

        RegisterPhaseCurrents();

        if (stopRequested)
        {
            Disable();
            return;
        }

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

        enabled = true;

        if (stopRequested)
            Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::Disable()
    {
        stopRequested = true;
        enabled = false;
        inverter.Stop();
        ReleasePhaseCurrents();
        foc.Disable();
    }

    template<typename FocImpl>
    void Runner<FocImpl>::RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer)
    {
        // The sampling interrupt calls the observer whenever the flag is set, so the observer must be complete before the flag
        // is, and the fence stops the compiler from reordering the plain write of the function past the volatile flag.
        observerRegistered = false;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        phaseCurrentsObserver = observer;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        observerRegistered = true;
    }

    template<typename FocImpl>
    void Runner<FocImpl>::UnregisterPhaseCurrentsObserver()
    {
        observerRegistered = false;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        phaseCurrentsObserver = nullptr;
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
        if (!enabled)
            return;

        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));

        if (observerRegistered)
            phaseCurrentsObserver(currentPhases);
    }
}
