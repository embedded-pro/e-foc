#pragma once

#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/foc/implementations/Runner.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include <utility>

namespace foc
{
    template<typename FocImpl>
    class FocController
        : public FocImpl
        , public Controllable
    {
    public:
        template<typename... Args>
        FocController(ThreePhaseInverter& inverter, Encoder& encoder, Args&&... args)
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

    private:
        Runner runner;
    };

    using FocTorqueController = FocController<FocTorqueImpl>;
    using FocSpeedController = FocController<FocSpeedImpl>;
    using FocPositionController = FocController<FocPositionImpl>;
}
