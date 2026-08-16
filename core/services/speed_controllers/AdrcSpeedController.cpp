#include "core/services/speed_controllers/AdrcSpeedController.hpp"
#include "core/services/speed_controllers/SpeedPlantModel.hpp"
#include <algorithm>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    // Toolbox asserts wo * Ts < 0.5 / Order; 0.4 stays under it and keeps both ESO poles real and damped
    constexpr float maxObserverBandwidthPerSample = 0.4f;
}

namespace services
{
    void AdrcSpeedController::Configure(const MechanicalModelParameters& motorParameters)
    {
        parameters = motorParameters;
        Construct();
    }

    void AdrcSpeedController::SetTunings(const SpeedControllerTunings& tunings)
    {
        bandwidth = tunings.bandwidth;
        observerBandwidthRatio = tunings.observerBandwidthRatio;
        Construct();
    }

    void AdrcSpeedController::Reset()
    {
        adrc.Reset();
        lastApplied = 0.0f;
    }

    OPTIMIZE_FOR_SPEED
    foc::Ampere AdrcSpeedController::Compute(const SpeedControlContext& context)
    {
        // Feeding the clipped current back keeps the disturbance estimate from winding up while the envelope is reached
        const auto applied = LimitToCurrentEnvelope(adrc.Compute(context.reference.Value(), context.measured.Value(), lastApplied),
            parameters.maxCurrent);
        lastApplied = applied.Value();

        return applied;
    }

    // Zero bandwidths hold both the observer and the control law at zero; b0 stays finite to keep the division defined
    AdrcSpeedController::SpeedAdrc AdrcSpeedController::Inert()
    {
        return { 0.0f, 0.0f, 1.0f, 1.0f };
    }

    void AdrcSpeedController::Construct()
    {
        if (!AreMechanicalParametersValid(parameters) || observerBandwidthRatio <= 0.0f || bandwidth <= 0.0f)
        {
            adrc = Inert();
            return;
        }

        const auto samplePeriod = OuterSamplePeriod(parameters.samplingFrequency);
        const auto observerBandwidth = std::min(observerBandwidthRatio * bandwidth, maxObserverBandwidthPerSample / samplePeriod);

        adrc = SpeedAdrc{ observerBandwidth, bandwidth, PlantInputGain(parameters), samplePeriod };
    }
}
