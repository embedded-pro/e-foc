#pragma once

#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/instantiations/Runner.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include <utility>

namespace foc
{
    template<typename FocImpl>
    class FocController
        : public FocImpl
        , public Controllable
        , public PhaseCurrentsObservable
    {
    public:
        template<typename... Args>
        FocController(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, Args&&... args)
            : FocImpl(std::forward<Args>(args)...)
            , runner(inverter, encoder, *this)
        {}

        void Start() override
        {
            runner.Enable();
        }

        void Stop() override
        {
            runner.Disable();
        }

        void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer) override
        {
            runner.RegisterPhaseCurrentsObserver(observer);
        }

        void UnregisterPhaseCurrentsObserver() override
        {
            runner.UnregisterPhaseCurrentsObserver();
        }

    private:
        Runner runner;
    };

    using FocTorqueController = FocController<TorqueCascade>;
    using FocSpeedController = FocController<SpeedCascade>;
    using FocPositionController = FocController<PositionCascade>;
}
