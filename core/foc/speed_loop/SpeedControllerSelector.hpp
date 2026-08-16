#pragma once

#include "core/foc/selection/ControllerSelector.hpp"
#include "core/foc/speed_loop/AdrcSpeedController.hpp"
#include "core/foc/speed_loop/LqiSpeedController.hpp"
#include "core/foc/speed_loop/PidSpeedController.hpp"
#include "core/foc/speed_loop/TwoDofSpeedController.hpp"

namespace foc
{
    struct SpeedControllerTraits
    {
        using Algorithm = SpeedAlgorithm;
        using Parameters = MechanicalModelParameters;
        using Tunings = SpeedLoopTunings;
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
