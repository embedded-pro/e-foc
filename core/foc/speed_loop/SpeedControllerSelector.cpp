#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

namespace foc
{
    bool SpeedControllerTraits::IsSelectable(SpeedAlgorithm algorithm, const MechanicalModelParameters& parameters, const SpeedLoopTunings&)
    {
        switch (algorithm)
        {
            case SpeedAlgorithm::lqi:
            case SpeedAlgorithm::adrc:
                return AreMechanicalParametersValid(parameters);
            default:
                return true;
        }
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
