#include "core/services/cogging_compensation/CoggingMapBuilder.hpp"
#include "core/foc/math/CoggingFeedForward.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr float countsPerHalfRange{ 127.0f };
        constexpr int peakSamplesPerBin{ 4 };
    }

    foc::NewtonMeter CoggingPeak(const std::array<int8_t, coggingMapBins>& torque, float torqueScale)
    {
        constexpr auto bins = coggingMapBins;
        float peak{ 0.0f };
        for (std::size_t bin = 0; bin != bins; ++bin)
            for (int step = 0; step != peakSamplesPerBin; ++step)
            {
                const auto t = static_cast<float>(step) / static_cast<float>(peakSamplesPerBin);
                const auto counts = foc::CatmullRom(torque[(bin + bins - 1) % bins], torque[bin], torque[(bin + 1) % bins], torque[(bin + 2) % bins], t);
                peak = std::max(peak, std::abs(counts));
            }

        return foc::NewtonMeter{ peak * torqueScale };
    }

    void CoggingMapBuilder::Restart()
    {
        accumulated = {};
    }

    std::optional<CoggingMap> CoggingMapBuilder::Build(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, std::size_t polePairs, float samplePeriod) const
    {
        if (polePairs == 0 || !foc::IsFiniteValue(torqueConstant.Value()) || torqueConstant.Value() <= 0.0f || samplePeriod <= 0.0f)
            return std::nullopt;

        const auto binWidth = 2.0f * std::numbers::pi_v<float> / static_cast<float>(bins * polePairs);
        const auto forward = DirectionTorque(Direction::forward, torqueConstant.Value(), inertia.Value(), binWidth, samplePeriod);
        const auto backward = DirectionTorque(Direction::backward, torqueConstant.Value(), inertia.Value(), binWidth, samplePeriod);

        if (!forward || !backward)
            return std::nullopt;

        const auto agreement = Agreement(*forward, *backward);
        if (agreement < minimumAgreement)
            return std::nullopt;

        Profile cogging{};
        float mean{ 0.0f };
        for (std::size_t bin = 0; bin != bins; ++bin)
        {
            cogging[bin] = 0.5f * ((*forward)[bin] + (*backward)[bin]);
            mean += cogging[bin];
        }
        mean /= static_cast<float>(bins);

        float largestBin{ 0.0f };
        for (auto& torque : cogging)
        {
            torque -= mean;
            largestBin = std::max(largestBin, std::abs(torque));
        }

        if (!foc::IsFiniteValue(largestBin) || largestBin <= 0.0f)
            return std::nullopt;

        CoggingMap map{};
        map.torqueScale = largestBin / countsPerHalfRange;
        for (std::size_t bin = 0; bin != bins; ++bin)
            map.torque[bin] = static_cast<int8_t>(std::lround(cogging[bin] / map.torqueScale));
        map.peak = CoggingPeak(map.torque, map.torqueScale);
        map.agreement = agreement;

        return map;
    }

    float CoggingMapBuilder::Agreement(const Profile& forward, const Profile& backward)
    {
        float forwardMean{ 0.0f };
        float backwardMean{ 0.0f };
        for (std::size_t bin = 0; bin != bins; ++bin)
        {
            forwardMean += forward[bin];
            backwardMean += backward[bin];
        }
        forwardMean /= static_cast<float>(bins);
        backwardMean /= static_cast<float>(bins);

        float shared{ 0.0f };
        float differing{ 0.0f };
        for (std::size_t bin = 0; bin != bins; ++bin)
        {
            const auto sum = (forward[bin] - forwardMean) + (backward[bin] - backwardMean);
            const auto difference = (forward[bin] - forwardMean) - (backward[bin] - backwardMean);
            shared += sum * sum;
            differing += difference * difference;
        }

        if (!foc::IsFiniteValue(shared) || !foc::IsFiniteValue(differing) || shared <= 0.0f)
            return 0.0f;

        constexpr float largestAgreementSquared{ 1.0e6f };
        return std::sqrt(shared / std::max(differing, shared / largestAgreementSquared));
    }

    std::optional<CoggingMapBuilder::Profile> CoggingMapBuilder::DirectionTorque(Direction direction, float torqueConstant, float inertia, float binWidth, float samplePeriod) const
    {
        const auto& profile = accumulated[static_cast<std::size_t>(direction)];
        const auto sign = direction == Direction::forward ? 1.0f : -1.0f;

        Profile speed{};
        for (std::size_t bin = 0; bin != bins; ++bin)
        {
            if (profile[bin].samples < minimumSamplesPerBin)
                return std::nullopt;

            speed[bin] = profile[bin].rotationSum / (static_cast<float>(profile[bin].samples) * samplePeriod);
            if (sign * speed[bin] <= 0.0f)
                return std::nullopt;
        }

        Profile torque{};
        for (std::size_t bin = 0; bin != bins; ++bin)
        {
            const auto ahead = speed[(bin + 1) % bins];
            const auto farAhead = speed[(bin + 2) % bins];
            const auto behind = speed[(bin + bins - 1) % bins];
            const auto farBehind = speed[(bin + bins - 2) % bins];
            const auto acceleration = speed[bin] * (8.0f * (ahead - behind) - (farAhead - farBehind)) / (12.0f * binWidth);
            const auto meanCurrent = profile[bin].currentSum / static_cast<float>(profile[bin].samples);
            torque[bin] = torqueConstant * meanCurrent - inertia * acceleration;
        }

        return torque;
    }
}
