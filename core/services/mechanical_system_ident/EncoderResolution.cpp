#include "core/services/mechanical_system_ident/EncoderResolution.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <limits>
#include <numbers>

namespace services
{
    namespace
    {
        constexpr float accelerationQuantumTarget{ 100.0f };
        constexpr std::size_t stepChangeMemory{ 4096 };
        constexpr float stepChangeToResolution{ std::numbers::sqrt2_v<float> };
    }

    void EncoderResolution::Restart(float position)
    {
        lastPosition = position;
        smallestStep = std::numeric_limits<float>::infinity();
        steps = 0;
        lastStep = 0.0f;
        stepChangeEnergy = 0.0f;
        stepChanges = 0;
        started = false;
    }

    void EncoderResolution::Sample(float position)
    {
        const auto signedStep = math::WrapToPi(position - lastPosition);
        const auto step = math::Abs(signedStep);
        lastPosition = position;

        if (started)
        {
            const auto change = signedStep - lastStep;
            ++stepChanges;
            stepChangeEnergy += (change * change - stepChangeEnergy) / static_cast<float>(std::min(stepChanges, stepChangeMemory));
        }

        started = true;
        lastStep = signedStep;

        if (step > 0.0f)
        {
            smallestStep = std::min(smallestStep, step);
            ++steps;
        }
    }

    bool EncoderResolution::Known() const
    {
        return steps >= minimumSteps;
    }

    std::size_t EncoderResolution::WindowSamples(float samplingPeriod) const
    {
        const auto stepChangeResolution = stepChanges >= minimumSteps ? stepChangeToResolution * math::Sqrt(stepChangeEnergy) : 0.0f;
        const auto resolution = std::max(smallestStep, stepChangeResolution);
        if (!math::IsFinite(resolution))
            return 1;

        const auto seconds = math::Sqrt(resolution / accelerationQuantumTarget);
        const auto samples = static_cast<std::size_t>(math::Ceil(seconds / samplingPeriod));
        return std::clamp<std::size_t>(samples, 1, maximumWindowSamples);
    }
}
