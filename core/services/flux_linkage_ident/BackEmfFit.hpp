#pragma once

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
            ++count;
            const auto weight = 1.0f / static_cast<float>(count);

            const auto speedDeviation = electricalSpeed - meanSpeed;
            const auto voltageDeviation = voltage - meanVoltage;
            meanSpeed += speedDeviation * weight;
            meanVoltage += voltageDeviation * weight;

            speedVariance += speedDeviation * (electricalSpeed - meanSpeed);
            covariance += speedDeviation * (voltage - meanVoltage);
            voltageVariance += voltageDeviation * (voltage - meanVoltage);

            lowestSpeed = std::min(lowestSpeed, std::abs(electricalSpeed));
            highestSpeed = std::max(highestSpeed, std::abs(electricalSpeed));
            largestVoltage = std::max(largestVoltage, std::abs(voltage));
        }

        uint32_t Count() const
        {
            return count;
        }

        float Slope() const
        {
            return speedVariance > 0.0f ? covariance / speedVariance : 0.0f;
        }

        float Intercept() const
        {
            return meanVoltage - Slope() * meanSpeed;
        }

        float RelativeSlopeError() const
        {
            const auto slope = Slope();

            if (count < 3 || speedVariance <= 0.0f || slope == 0.0f)
                return std::numeric_limits<float>::infinity();

            const auto residual = std::max(voltageVariance - slope * covariance, 0.0f);
            const auto standardError = std::sqrt(residual / static_cast<float>(count - 2) / speedVariance);

            return standardError / std::abs(slope);
        }

        float LowestSpeed() const
        {
            return count != 0 ? lowestSpeed : 0.0f;
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
        uint32_t count{ 0 };
        float meanSpeed{ 0.0f };
        float meanVoltage{ 0.0f };
        float speedVariance{ 0.0f };
        float covariance{ 0.0f };
        float voltageVariance{ 0.0f };
        float lowestSpeed{ std::numeric_limits<float>::infinity() };
        float highestSpeed{ 0.0f };
        float largestVoltage{ 0.0f };
    };
}
