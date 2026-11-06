#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <optional>

namespace services
{
    class IndexSearch
    {
    protected:
        ~IndexSearch() = default;

    public:
        struct Config
        {
            std::size_t polePairs{ 1 };
            foc::Ohm phaseResistance{ 0.0f };
            foc::Volts busVoltage{ 0.0f };
            foc::Ampere current{ 0.6f };
            // Mechanical angle the encoder is set to read at the index; none only measures it
            std::optional<foc::Radians> target;
        };

        virtual bool Available() const = 0;
        virtual void Search(const Config& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) = 0;
        virtual void Abort() = 0;
        virtual bool IsRunning() const = 0;
    };
}
