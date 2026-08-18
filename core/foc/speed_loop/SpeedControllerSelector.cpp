#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/speed_loop/SpeedControllerSelector.hpp"
#include "core/foc/speed_loop/SpeedPlantModel.hpp"

namespace foc
{
    // Only the model-based laws need the mechanical set; PID and Two-DOF stay selectable before
    // mechanical identification and simply hold their output at zero until gains arrive (REQ-CTRL-012).
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
