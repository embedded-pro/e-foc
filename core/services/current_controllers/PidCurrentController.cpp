#include "core/services/current_controllers/PidCurrentController.hpp"
#include "core/services/current_controllers/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace services
{
    void PidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        ApplyGains();
    }

    void PidCurrentController::SetTunings(const CurrentControllerTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        ApplyGains();
    }

    void PidCurrentController::Reset()
    {
        dPid.Reset();
        qPid.Reset();
    }

    OPTIMIZE_FOR_SPEED
    foc::RotatingFrame PidCurrentController::Compute(const CurrentControlContext& context)
    {
        dPid.SetPoint(context.reference.d);
        qPid.SetPoint(context.reference.q);

        return LimitToModulationCircle({ dPid.Process(context.measured.d), qPid.Process(context.measured.q) });
    }

    void PidCurrentController::ApplyGains()
    {
        if (!AreElectricalParametersValid(parameters))
            return;

        const auto scale = NormalizationScale(parameters.busVoltage);
        const auto kp = InductanceInHenry(parameters.inductance) * bandwidth * scale;
        const auto ki = parameters.resistance.Value() * bandwidth * scale;

        dPid.SetTunings({ kp, ki, 0.0f });
        qPid.SetTunings({ kp, ki, 0.0f });
    }
}
