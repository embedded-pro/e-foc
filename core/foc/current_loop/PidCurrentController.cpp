#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/current_loop/PidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

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
        dPi.Reset();
        qPi.Reset();
    }

    void PidCurrentController::ApplyGains()
    {
        if (!AreElectricalParametersValid(parameters))
        {
            dPi.SetTunings(0.0f, 0.0f);
            qPi.SetTunings(0.0f, 0.0f);
            return;
        }

        const auto scale = NormalizationScale(parameters.busVoltage);
        const auto kp = InductanceInHenry(parameters.inductance) * bandwidth * scale;
        const auto ki = parameters.resistance.Value() * bandwidth * scale * SamplePeriod(parameters.samplingFrequency);

        dPi.SetTunings(kp, ki);
        qPi.SetTunings(kp, ki);
    }
}
