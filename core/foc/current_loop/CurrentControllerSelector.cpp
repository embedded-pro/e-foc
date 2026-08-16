#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    bool CurrentControllerTraits::IsSelectable(CurrentAlgorithm algorithm, const MotorModelParameters& parameters, const CurrentLoopTunings&)
    {
        if (!AreElectricalParametersValid(parameters))
            return false;

        return algorithm != CurrentAlgorithm::decoupledPid || parameters.fluxLinkage.Value() > 0.0f;
    }

    SelectResult CurrentControllerSelector::Select(CurrentAlgorithm algorithm)
    {
        using enum CurrentAlgorithm;

        switch (algorithm)
        {
            case pid:
                return Select<PidCurrentController>();
            case decoupledPid:
                return Select<DecoupledPidCurrentController>();
            case deadbeat:
                return Select<DeadbeatCurrentController>();
            case slidingMode:
                return Select<SlidingModeCurrentController>();
            default:
                return SelectResult::invalidAlgorithm;
        }
    }
}
