#pragma once

#include "integration_tests/support/response/ResponseWindow.hpp"
#include "numerical/math/StepResponseMetrics.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace integration::response
{
    struct StepResponseMetrics
    {
        float riseTimeS;
        float settlingTimeS;
        float percentOvershoot;
        float peakTimeS;
        float steadyStateError;
        float tailBandPercent;
    };

    struct DisturbanceResponseMetrics
    {
        float maxDeviation;
        float recoveryTimeS;
        float steadyStateError;
    };

    template<std::size_t N>
    std::optional<StepResponseMetrics> ComputeStepMetrics(const std::vector<float>& normalised, float dtSeconds, float band, float stepMagnitude)
    {
        if (normalised.size() < N)
            return std::nullopt;

        math::Vector<float, N> v;
        float tailBand = 0.0f;
        for (std::size_t i = 0; i != N; ++i)
        {
            v.at(i, 0) = normalised[i];
            if (i >= N - N / 4)
                tailBand = std::max(tailBand, std::abs(normalised[i] - 1.0f));
        }

        return StepResponseMetrics{
            math::RiseTime(v, 1.0f, dtSeconds),
            math::SettlingTime(v, 1.0f, band, dtSeconds),
            math::PercentOvershoot(v, 1.0f),
            math::PeakTime(v, dtSeconds),
            math::SteadyStateError(v, 1.0f) * stepMagnitude,
            100.0f * tailBand,
        };
    }

    template<std::size_t N>
    std::optional<DisturbanceResponseMetrics> ComputeDisturbanceMetrics(const std::vector<float>& raw, float reference, float dtSeconds, float absoluteBand)
    {
        if (raw.size() < N || reference == 0.0f)
            return std::nullopt;

        math::Vector<float, N> v;
        float maxDeviation = 0.0f;
        for (std::size_t i = 0; i != N; ++i)
        {
            v.at(i, 0) = raw[i];
            maxDeviation = std::max(maxDeviation, std::abs(raw[i] - reference));
        }

        return DisturbanceResponseMetrics{
            maxDeviation,
            math::SettlingTime(v, reference, absoluteBand / std::abs(reference), dtSeconds),
            math::SteadyStateError(v, reference),
        };
    }

    inline std::optional<StepResponseMetrics> ComputeStepMetricsFor(Signal signal, const std::vector<float>& normalised, float dtSeconds, float band, float stepMagnitude)
    {
        switch (signal)
        {
            case Signal::speed:
                return ComputeStepMetrics<speedWindowSamples>(normalised, dtSeconds, band, stepMagnitude);
            case Signal::position:
                return ComputeStepMetrics<positionWindowSamples>(normalised, dtSeconds, band, stepMagnitude);
            case Signal::currentQ:
                return ComputeStepMetrics<currentWindowSamples>(normalised, dtSeconds, band, stepMagnitude);
        }
        return std::nullopt;
    }
}
