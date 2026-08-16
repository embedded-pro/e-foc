#pragma once

#include "core/services/controller_selection/ControllerSelector.hpp"
#include "core/services/current_controllers/DeadbeatCurrentController.hpp"
#include "core/services/current_controllers/DecoupledPidCurrentController.hpp"
#include "core/services/current_controllers/PidCurrentController.hpp"
#include "core/services/current_controllers/SlidingModeCurrentController.hpp"

namespace services
{
    struct CurrentControllerTraits
    {
        using Algorithm = CurrentAlgorithm;
        using Parameters = MotorModelParameters;
        using Tunings = CurrentControllerTunings;
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
