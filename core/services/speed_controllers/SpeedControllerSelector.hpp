#pragma once

#include "core/services/controller_selection/ControllerSelector.hpp"
#include "core/services/speed_controllers/AdrcSpeedController.hpp"
#include "core/services/speed_controllers/LqiSpeedController.hpp"
#include "core/services/speed_controllers/PidSpeedController.hpp"
#include "core/services/speed_controllers/TwoDofSpeedController.hpp"

namespace services
{
    struct SpeedControllerTraits
    {
        using Algorithm = SpeedAlgorithm;
        using Parameters = MechanicalModelParameters;
        using Tunings = SpeedControllerTunings;
        using Context = SpeedControlContext;
        using Output = foc::Ampere;

        static bool IsSelectable(Algorithm algorithm, const Parameters& parameters);
    };

    class SpeedControllerSelector
        : public ControllerSelector<SpeedControllerTraits,
              PidSpeedController,
              LqiSpeedController,
              AdrcSpeedController,
              TwoDofSpeedController>
    {
    public:
        using ControllerSelector::Select;

        SelectResult Select(SpeedAlgorithm algorithm);
    };
}
