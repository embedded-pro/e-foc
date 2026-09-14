#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/current_loop/PidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

namespace foc
{
    bool PidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        parameters = motorParameters;
        return ApplyGains();
    }

    bool PidCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        return ApplyGains();
    }

    void PidCurrentController::Reset()
    {
        dPi.Reset();
        qPi.Reset();
    }

    bool PidCurrentController::ApplyGains()
    {
        if (!AreElectricalParametersValid(parameters))
        {
            dPi.SetTunings(0.0f, 0.0f);
            qPi.SetTunings(0.0f, 0.0f);
            return false;
        }

        const auto scale = NormalizationScale(parameters.busVoltage);
        const auto kp = InductanceInHenry(parameters.inductance) * bandwidth * scale;
        const auto ki = parameters.resistance.Value() * bandwidth * scale * SamplePeriod(parameters.samplingFrequency);

        dPi.SetTunings(kp, ki);
        qPi.SetTunings(kp, ki);
        return true;
    }
}
