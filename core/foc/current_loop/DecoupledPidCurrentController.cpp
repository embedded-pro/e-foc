#include "core/foc/current_loop/DecoupledPidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void DecoupledPidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        pid.Configure(motorParameters);

        if (!AreElectricalParametersValid(motorParameters))
            return;

        const auto scale = NormalizationScale(motorParameters.busVoltage);
        couplingScale = InductanceInHenry(motorParameters.inductance) * scale;
        backEmfScale = motorParameters.fluxLinkage.Value() * scale;
    }

    void DecoupledPidCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        pid.SetTunings(tunings);
    }

    void DecoupledPidCurrentController::Reset()
    {
        pid.Reset();
    }

    OPTIMIZE_FOR_SPEED
    foc::RotatingFrame DecoupledPidCurrentController::Compute(const CurrentControlContext& context)
    {
        const auto feedback = pid.Compute(context);
        const auto electricalSpeed = context.electricalSpeed;

        const auto dFeedforward = -electricalSpeed * couplingScale * context.measured.q;
        const auto qFeedforward = electricalSpeed * (couplingScale * context.measured.d + backEmfScale);

        return LimitToModulationCircle({ feedback.d + dFeedforward, feedback.q + qFeedforward });
    }
}
