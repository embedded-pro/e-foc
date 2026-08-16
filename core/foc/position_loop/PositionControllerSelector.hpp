#pragma once

#include "core/foc/position_loop/CascadePPositionController.hpp"
#include "core/foc/position_loop/LqiPositionController.hpp"
#include "core/foc/position_loop/LqrPositionController.hpp"
#include "core/foc/position_loop/PidPositionController.hpp"
#include "core/foc/position_loop/TwoDofPositionController.hpp"
#include "core/foc/selection/ControllerSelector.hpp"

namespace foc
{
    struct PositionControllerTraits
    {
        using Algorithm = PositionAlgorithm;
        using Parameters = MechanicalModelParameters;
        using Tunings = PositionLoopTunings;
        using Context = PositionControlContext;
        using Output = PositionOutput;

        static bool IsSelectable(Algorithm algorithm, const Parameters& parameters, const Tunings& tunings);
    };

    class PositionControllerSelector
        : public ControllerSelector<PositionControllerTraits,
              PidPositionController,
              CascadePPositionController,
              LqrPositionController,
              LqiPositionController,
              TwoDofPositionController>
    {
    public:
        using ControllerSelector::Select;

        SelectResult Select(PositionAlgorithm algorithm);
    };
}
