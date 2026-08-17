#include "core/foc/current_loop/PidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void PidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        ApplyGains();
    }

    void PidCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        ApplyGains();
    }

    void PidCurrentController::Reset()
    {
        dPid.Reset();
        qPid.Reset();
    }

    void PidCurrentController::ApplyGains()
    {
        if (!AreElectricalParametersValid(parameters))
            return;

        const auto scale = NormalizationScale(parameters.busVoltage);
        const auto kp = InductanceInHenry(parameters.inductance) * bandwidth * scale;
        const auto ki = parameters.resistance.Value() * bandwidth * scale * SamplePeriod(parameters.samplingFrequency);

        dPid.SetTunings({ kp, ki, 0.0f });
        qPid.SetTunings({ kp, ki, 0.0f });
    }
}
