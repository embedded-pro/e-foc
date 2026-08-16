#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <concepts>
#include <cstdint>

namespace services
{
    enum class SpeedAlgorithm : uint8_t
    {
        pid = 0,
        lqi = 1,
        adrc = 2,
        twoDof = 3
    };

    struct MechanicalModelParameters
    {
        foc::NewtonMeterSecondSquared inertia{ 0.0f };
        foc::NewtonMeterSecondPerRadian viscousFriction{ 0.0f };
        foc::NewtonMeter torqueConstant{ 0.0f };
        foc::Ampere maxCurrent{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
    };

    struct SpeedControllerTunings
    {
        float bandwidth{ 188.495559f };
        float speedErrorWeight{ 1.0f };
        float integralWeight{ 0.1f };
        float observerBandwidthRatio{ 5.0f };
        float referenceTimeConstant{ 0.0053051f };
    };

    struct SpeedControlContext
    {
        foc::RadiansPerSecond measured{ 0.0f };
        foc::RadiansPerSecond reference{ 0.0f };
    };

    template<typename T>
    concept SpeedController = requires(T controller,
        const MechanicalModelParameters& parameters,
        const SpeedControllerTunings& tunings,
        const SpeedControlContext& context) {
        { T::algorithm } -> std::convertible_to<SpeedAlgorithm>;
        { controller.Configure(parameters) } -> std::same_as<void>;
        { controller.SetTunings(tunings) } -> std::same_as<void>;
        { controller.Reset() } -> std::same_as<void>;
        { controller.Compute(context) } -> std::same_as<foc::Ampere>;
    };
}
