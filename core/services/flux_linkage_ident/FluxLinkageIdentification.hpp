#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace services
{
    class FluxLinkageIdentification
    {
    protected:
        ~FluxLinkageIdentification() = default;

    public:
        struct Config
        {
            foc::Ampere spinCurrent{ 0.6f };
            foc::RadiansPerSecond releaseSpeed{ 40.0f };
            foc::RadiansPerSecond maxSpeed{ 50.0f };
            foc::RadiansPerSecond lowestSpeed{ 10.0f };
            foc::RadiansPerSecond stoppedSpeed{ 3.0f };
            infra::Duration settleTime{ std::chrono::milliseconds{ 10 } };
            infra::Duration coastLimit{ std::chrono::milliseconds{ 1500 } };
            infra::Duration brakeLimit{ std::chrono::milliseconds{ 500 } };
            infra::Duration timeout{ std::chrono::seconds{ 5 } };
            infra::Duration pointPeriod{ std::chrono::milliseconds{ 10 } };
            uint32_t requiredSamples{ 30 };
            uint8_t maxCoasts{ 6 };
            float maxRelativeError{ 0.05f };
            float maxInterceptFraction{ 0.15f };
        };

        enum class Outcome : uint8_t
        {
            identified,
            notIdentified,
            overspeed
        };

        struct Result
        {
            Outcome outcome{ Outcome::notIdentified };
            foc::Weber fluxLinkage{ 0.0f };
            uint32_t samples{ 0 };
            float relativeError{ 0.0f };
            foc::Volts intercept{ 0.0f };
        };

        virtual void Identify(std::size_t polePairs, foc::Ohm resistance, const Config& config, const infra::Function<void(const Result& result)>& onDone) = 0;
        virtual bool IsRunning() const = 0;
        virtual void Abort() = 0;
    };
}
