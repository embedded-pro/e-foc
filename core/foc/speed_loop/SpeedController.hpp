#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <concepts>
#include <cstdint>

namespace foc
{
    enum class SpeedAlgorithm : uint8_t
    {
        pid = 0,
        lqi = 1,
        adrc = 2,
        twoDof = 3
    };

    struct SpeedControlContext
    {
        RadiansPerSecond measured{ 0.0f };
        RadiansPerSecond reference{ 0.0f };
    };

    template<typename T>
    concept SpeedController = requires(T controller,
        const MechanicalModelParameters& parameters,
        const SpeedLoopTunings& tunings,
        const SpeedControlContext& context) {
        { T::algorithm } -> std::convertible_to<SpeedAlgorithm>;
        { controller.Configure(parameters) } -> std::same_as<void>;
        { controller.SetTunings(tunings) } -> std::same_as<void>;
        { controller.Reset() } -> std::same_as<void>;
        { controller.Compute(context) } -> std::same_as<Ampere>;
    };
}
