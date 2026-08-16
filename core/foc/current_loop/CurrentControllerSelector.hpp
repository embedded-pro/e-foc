#pragma once

#include "core/foc/current_loop/DeadbeatCurrentController.hpp"
#include "core/foc/current_loop/DecoupledPidCurrentController.hpp"
#include "core/foc/current_loop/PidCurrentController.hpp"
#include "core/foc/current_loop/SlidingModeCurrentController.hpp"
#include "core/foc/selection/ControllerSelector.hpp"

namespace foc
{
    struct CurrentControllerTraits
    {
        using Algorithm = CurrentAlgorithm;
        using Parameters = MotorModelParameters;
        using Tunings = CurrentLoopTunings;
        using Context = CurrentControlContext;
        using Output = foc::RotatingFrame;

        static bool IsSelectable(Algorithm algorithm, const Parameters& parameters);
    };

    class CurrentControllerSelector
        : public ControllerSelector<CurrentControllerTraits,
              PidCurrentController,
              DecoupledPidCurrentController,
              DeadbeatCurrentController,
              SlidingModeCurrentController>
    {
    public:
        using ControllerSelector::Select;

        SelectResult Select(CurrentAlgorithm algorithm);
    };
}
