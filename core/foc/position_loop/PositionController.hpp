#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include <concepts>

namespace foc
{
    struct PositionControlContext
    {
        Radians measured{ 0.0f };
        Radians reference{ 0.0f };
        RadiansPerSecond measuredSpeed{ 0.0f };
        // Set within software position limits: both angles stay between them, so no difference is folded.
        bool direct{ false };
    };

    struct PositionOutput
    {
        PositionOutputKind kind{ PositionOutputKind::speedReference };
        float value{ 0.0f };
    };

    template<typename T>
    concept PositionController = requires(T controller,
        const MechanicalModelParameters& parameters,
        const PositionLoopTunings& tunings,
        const PositionControlContext& context) {
        { T::algorithm } -> std::convertible_to<PositionAlgorithm>;
        { controller.Configure(parameters) } -> std::same_as<bool>;
        { controller.SetTunings(tunings) } -> std::same_as<bool>;
        { controller.Reset() } -> std::same_as<void>;
        { controller.Compute(context) } -> std::same_as<PositionOutput>;
    };
}
