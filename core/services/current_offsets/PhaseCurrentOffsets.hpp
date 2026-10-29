#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "infra/util/Function.hpp"
#include <optional>

namespace services
{
    class PhaseCurrentOffsets
    {
    protected:
        ~PhaseCurrentOffsets() = default;

    public:
        virtual void Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone) = 0;
        virtual void Apply(const foc::PhaseCurrents& offsets) = 0;
        virtual foc::Ampere Noise() const = 0;
        virtual bool IsRunning() const = 0;
        virtual void Abort() = 0;
    };
}
