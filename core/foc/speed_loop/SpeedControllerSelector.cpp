#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    // Every speed algorithm derives its gains from the same mechanical set, so the rule is uniform
    bool SpeedControllerTraits::IsSelectable(SpeedAlgorithm, const MechanicalModelParameters& parameters, const SpeedLoopTunings&)
    {
        return AreMechanicalParametersValid(parameters);
    }

    SelectResult SpeedControllerSelector::Select(SpeedAlgorithm algorithm)
    {
        using enum SpeedAlgorithm;

        switch (algorithm)
        {
            case pid:
                return Select<PidSpeedController>();
            case lqi:
                return Select<LqiSpeedController>();
            case adrc:
                return Select<AdrcSpeedController>();
            case twoDof:
                return Select<TwoDofSpeedController>();
            default:
                return SelectResult::invalidAlgorithm;
        }
    }
}
