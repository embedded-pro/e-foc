#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <variant>

namespace foc
{
    template<typename T, typename Algorithm>
    concept AlgorithmOf = std::same_as<std::remove_const_t<decltype(T::algorithm)>, Algorithm>;

    // Holds every controller of one loop in fixed-size storage and dispatches by variant tag, so the
    // hot path resolves to a direct call. Traits supplies the loop's types plus its readiness rule.
    template<typename Traits, typename... Controllers>
    class ControllerSelector
    {
    public:
        using Algorithm = typename Traits::Algorithm;
        using Parameters = typename Traits::Parameters;
        using Tunings = typename Traits::Tunings;
        using Context = typename Traits::Context;
        using Output = typename Traits::Output;

        template<AlgorithmOf<Algorithm> T>
        SelectResult Select()
        {
            if (!Traits::IsSelectable(T::algorithm, parameters, tunings))
                return SelectResult::invalidParameters;

            active.template emplace<T>();
            activeAlgorithm = T::algorithm;
            ApplyConfiguration();
            Reset();

            return SelectResult::ok;
        }

        Algorithm Active() const
        {
            return activeAlgorithm;
        }

        void Configure(const Parameters& motorParameters)
        {
            parameters = motorParameters;
            ApplyConfiguration();
        }

        void SetTunings(const Tunings& controllerTunings)
        {
            tunings = controllerTunings;
            ApplyConfiguration();
        }

        // Rejects tunings the active algorithm cannot be designed for, leaving the last accepted set live
        SelectResult TrySetTunings(const Tunings& controllerTunings)
        {
            if (!Traits::IsSelectable(activeAlgorithm, parameters, controllerTunings))
                return SelectResult::invalidParameters;

            SetTunings(controllerTunings);

            return SelectResult::ok;
        }

        void Reset()
        {
            std::visit([](auto& controller)
                {
                    controller.Reset();
                },
                active);
        }

        OPTIMIZE_FOR_SPEED Output Compute(const Context& context)
        {
            return std::visit([&context](auto& controller)
                {
                    return controller.Compute(context);
                },
                active);
        }

    private:
        using Storage = std::variant<Controllers...>;

        void ApplyConfiguration()
        {
            std::visit([this](auto& controller)
                {
                    controller.Configure(parameters);
                    controller.SetTunings(tunings);
                },
                active);
        }

        Storage active;
        Algorithm activeAlgorithm{ std::variant_alternative_t<0, Storage>::algorithm };
        Parameters parameters{};
        Tunings tunings{};
    };
}
