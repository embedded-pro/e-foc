#pragma once

#include "numerical/estimators/online/RunningLinearFit.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace services
{
    class BackEmfFit
    {
    public:
        void Add(float electricalSpeed, float voltage)
        {
            line.Add(electricalSpeed, voltage);

            lowestSpeed = std::min(lowestSpeed, math::Abs(electricalSpeed));
            highestSpeed = std::max(highestSpeed, math::Abs(electricalSpeed));
            largestVoltage = std::max(largestVoltage, math::Abs(voltage));
        }

        uint32_t Count() const
        {
            return line.Count();
        }

        float Slope() const
        {
            return line.Slope();
        }

        float Intercept() const
        {
            return line.Intercept();
        }

        float RelativeSlopeError() const
        {
            return line.RelativeSlopeError();
        }

        float LowestSpeed() const
        {
            return line.Count() != 0 ? lowestSpeed : 0.0f;
        }

        float HighestSpeed() const
        {
            return highestSpeed;
        }

        float LargestVoltage() const
        {
            return largestVoltage;
        }

    private:
        estimators::RunningLinearFit<float> line;
        float lowestSpeed{ std::numeric_limits<float>::infinity() };
        float highestSpeed{ 0.0f };
        float largestVoltage{ 0.0f };
    };
}
