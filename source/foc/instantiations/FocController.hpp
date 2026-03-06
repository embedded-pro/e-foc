#pragma once

#include "source/foc/implementations/FocPositionImpl.hpp"
#include "source/foc/implementations/FocSpeedImpl.hpp"
#include "source/foc/implementations/FocTorqueImpl.hpp"
#include "source/foc/implementations/Runner.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Foc.hpp"
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
