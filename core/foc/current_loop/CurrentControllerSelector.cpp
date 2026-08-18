#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/current_loop/CurrentControllerSelector.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

namespace foc
{
    bool CurrentControllerTraits::IsSelectable(CurrentAlgorithm algorithm, const MotorModelParameters& parameters, const CurrentLoopTunings&)
    {
        if (!AreElectricalParametersValid(parameters))
            return false;

        const bool requiresFluxLinkage = algorithm == CurrentAlgorithm::decoupledPid || algorithm == CurrentAlgorithm::deadbeat;
        return !requiresFluxLinkage || parameters.fluxLinkage.Value() > 0.0f;
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
