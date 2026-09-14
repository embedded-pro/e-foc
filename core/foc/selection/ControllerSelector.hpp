#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <concepts>
#include <type_traits>
#include <variant>

namespace foc
{
    template<typename T, typename Algorithm>
    concept AlgorithmOf = std::same_as<std::remove_const_t<decltype(T::algorithm)>, Algorithm>;

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

        bool Configure(const Parameters& motorParameters)
        {
            parameters = motorParameters;
            return ApplyConfiguration();
        }

        bool SetTunings(const Tunings& controllerTunings)
        {
            tunings = controllerTunings;
            return ApplyConfiguration();
        }

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

        bool ApplyConfiguration()
        {
            return std::visit([this](auto& controller)
                {
                    const bool configOk = controller.Configure(parameters);
                    const bool tuningsOk = controller.SetTunings(tunings);
                    return configOk && tuningsOk;
                },
                active);
        }

        Storage active;
        Algorithm activeAlgorithm{ std::variant_alternative_t<0, Storage>::algorithm };
        Parameters parameters{};
        Tunings tunings{};
    };
}
