#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/IsrProfile.hpp"
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
        infra::HotSwapFunction<void(const PhaseCurrents& currentPhases)> phaseCurrentsObserver;
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
        if (!enabled)
            return;

        diag::isrProfile.Mark(diag::IsrStage::dispatch);
        auto position = encoder.Read();
        diag::isrProfile.Mark(diag::IsrStage::encoder);
        const auto duties = foc.Calculate(currentPhases, position);
        diag::isrProfile.Mark(diag::IsrStage::calculateRest);
        inverter.ThreePhasePwmOutput(duties);
        diag::isrProfile.Mark(diag::IsrStage::pwm);

        phaseCurrentsObserver(currentPhases);
        diag::isrProfile.Mark(diag::IsrStage::observer);
    }
}
