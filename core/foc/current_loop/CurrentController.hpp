#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include <concepts>
#include <cstdint>

namespace foc
{
    enum class CurrentAlgorithm : uint8_t
    {
        pid = 0,
        decoupledPid = 1,
        deadbeat = 2,
        slidingMode = 3
    };

    struct CurrentControlContext
    {
        RotatingFrame measured{ 0.0f, 0.0f };
        RotatingFrame reference{ 0.0f, 0.0f };
        float electricalSpeed{ 0.0f };
    };

    template<typename T>
    concept CurrentController = requires(T controller,
        const MotorModelParameters& parameters,
        const CurrentLoopTunings& tunings,
        const CurrentControlContext& context) {
        { T::algorithm } -> std::convertible_to<CurrentAlgorithm>;
        { controller.Configure(parameters) } -> std::same_as<void>;
        { controller.SetTunings(tunings) } -> std::same_as<void>;
        { controller.Reset() } -> std::same_as<void>;
        { controller.Compute(context) } -> std::same_as<RotatingFrame>;
    };
}
