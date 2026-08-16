#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <concepts>
#include <cstdint>

namespace services
{
    enum class CurrentAlgorithm : uint8_t
    {
        pid = 0,
        decoupledPid = 1,
        deadbeat = 2,
        slidingMode = 3
    };

    struct MotorModelParameters
    {
        foc::Ohm resistance{ 0.0f };
        foc::MilliHenry inductance{ 0.0f };
        foc::Weber fluxLinkage{ 0.0f };
        foc::Volts busVoltage{ 0.0f };
        hal::Hertz samplingFrequency{ 0 };
    };

    struct CurrentControllerTunings
    {
        float bandwidth{ 6283.185307f };
        float switchingGain{ 1.0f };
        float boundaryLayer{ 0.2f };
        bool twoStepDeadbeat{ false };
    };

    struct CurrentControlContext
    {
        foc::RotatingFrame measured{ 0.0f, 0.0f };
        foc::RotatingFrame reference{ 0.0f, 0.0f };
        float electricalSpeed{ 0.0f };
    };

    template<typename T>
    concept CurrentController = requires(T controller,
        const MotorModelParameters& parameters,
        const CurrentControllerTunings& tunings,
        const CurrentControlContext& context) {
        { T::algorithm } -> std::convertible_to<CurrentAlgorithm>;
        { controller.Configure(parameters) } -> std::same_as<void>;
        { controller.SetTunings(tunings) } -> std::same_as<void>;
        { controller.Reset() } -> std::same_as<void>;
        { controller.Compute(context) } -> std::same_as<foc::RotatingFrame>;
    };
}
