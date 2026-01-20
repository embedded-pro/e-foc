#include "source/services/parameter_identification/MechanicalParametersIdentificationImpl.hpp"
#include "source/foc/interfaces/Units.hpp"
#include <cmath>
#include <numbers>

namespace
{
    const hal::Hertz samplingFrequency{ 10000 };
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float twoPi = 2.0f * pi;

    float PositionWithWrapAround(float position)
    {
        if (position > pi)
            position -= twoPi;
        else if (position < -pi)
            position += twoPi;

        return position;
    }

    float AverageAndRemoveFront(infra::BoundedDeque<float>& deque)
    {
        float sum = 0.0f;

        for (const auto& sample : deque)
            sum += sample;

        float average = sum / static_cast<float>(deque.size());

        deque.pop_front();

        return average;
    }

    float CalculateAverage(const infra::BoundedVector<float>& samples)
    {
        if (samples.empty())
            return 0.0f;

        float sum = 0.0f;
        for (const auto& sample : samples)
            sum += sample;

        return sum / static_cast<float>(samples.size());
    }

    std::optional<float> CalculateLinearSlope(const infra::BoundedVector<float>& xData, const infra::BoundedVector<float>& yData)
    {
        if (xData.size() != yData.size() || xData.size() < 2)
            return std::nullopt;

        float n = static_cast<float>(xData.size());
        float sumX = 0.0f;
        float sumY = 0.0f;
        float sumXY = 0.0f;
        float sumX2 = 0.0f;

        for (std::size_t i = 0; i < xData.size(); ++i)
        {
            sumX += xData[i];
            sumY += yData[i];
            sumXY += xData[i] * yData[i];
            sumX2 += xData[i] * xData[i];
        }

        float denominator = (n * sumX2 - sumX * sumX);
        if (std::abs(denominator) < 1e-6f)
            return std::nullopt;

        float slope = (n * sumXY - sumX * sumY) / denominator;
        return slope;
    }
}

namespace services
{
    MechanicalParametersIdentificationImpl::MechanicalParametersIdentificationImpl(foc::SpeedController& controller, foc::MotorDriver& driver, foc::Encoder& encoder)
        : controller(controller)
        , driver(driver)
        , encoder(encoder)
        , samplingPeriod(1.0f / static_cast<float>(samplingFrequency.Value()))
    {
    }

    void MechanicalParametersIdentificationImpl::EstimateFriction(const FrictionConfig& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>)>& onDone)
    {
        frictionConfig = config;
        onFrictionDone = onDone;
        rawCurrentSamples.clear();
        filteredCurrentSamples.clear();

        controller.Enable();
        controller.SetPoint(frictionConfig.targetSpeed);

        settleTimer.Start(frictionConfig.settleTime, [this]()
            {
                driver.PhaseCurrentsReady(samplingFrequency, [this](auto currentPhases)
                    {
                        rawCurrentSamples.push_back(currentPhases.c.Value());

                        if (rawCurrentSamples.full())
                            filteredCurrentSamples.push_back(AverageAndRemoveFront(rawCurrentSamples));

                        if (filteredCurrentSamples.full())
                            CalculateFriction();
                    });
            });
    }

    void MechanicalParametersIdentificationImpl::CalculateFriction()
    {
        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        controller.Disable();
        driver.Stop();

        if (filteredCurrentSamples.size() < 10)
        {
            onFrictionDone(std::nullopt);
            return;
        }

        float averageCurrent = CalculateAverage(filteredCurrentSamples);
        float averageTorque = averageCurrent * frictionConfig.torqueConstant.Value();
        float targetSpeedRadPerSec = frictionConfig.targetSpeed.Value();

        if (std::abs(targetSpeedRadPerSec) < 0.1f)
        {
            onFrictionDone(std::nullopt);
            return;
        }

        float damping = averageTorque / targetSpeedRadPerSec;
        onFrictionDone(foc::NewtonMeterSecondPerRadian{ damping });
    }

    void MechanicalParametersIdentificationImpl::EstimateInertia(const InertiaConfig& config, foc::NewtonMeterSecondPerRadian damping, const infra::Function<void(std::optional<foc::NewtonMeterSecondSquared>)>& onDone)
    {
        inertiaConfig = config;
        currentDamping = damping;
        onInertiaDone = onDone;
        speedSamples.clear();
        timeSamples.clear();
        sampleIndex = 0;
        previousPosition = encoder.Read().Value();

        controller.Disable();
        driver.Stop();

        settleTimer.Start(std::chrono::milliseconds{ 500 }, [this]()
            {
                controller.Enable();
                controller.SetPoint(foc::RadiansPerSecond{ 0.0f });

                driver.PhaseCurrentsReady(samplingFrequency, [this](auto)
                    {
                        CollectInertiaSamples();
                    });
            });
    }

    void MechanicalParametersIdentificationImpl::CollectInertiaSamples()
    {
        auto currentPosition = encoder.Read().Value();
        auto speed = PositionWithWrapAround(currentPosition - previousPosition) / samplingPeriod;
        previousPosition = currentPosition;

        speedSamples.push_back(speed);
        timeSamples.push_back(static_cast<float>(sampleIndex) * samplingPeriod);

        sampleIndex++;

        if (speedSamples.full())
        {
            driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
            CalculateInertia();
        }
    }

    void MechanicalParametersIdentificationImpl::CalculateInertia()
    {
        controller.Disable();
        driver.Stop();

        if (speedSamples.size() < 10 || timeSamples.size() < 10)
        {
            onInertiaDone(std::nullopt);
            return;
        }

        auto acceleration = CalculateLinearSlope(timeSamples, speedSamples);

        if (!acceleration.has_value() || std::abs(*acceleration) < 0.1f)
        {
            onInertiaDone(std::nullopt);
            return;
        }

        float appliedTorque = inertiaConfig.torqueStepCurrent.Value() * inertiaConfig.torqueConstant.Value();
        float averageSpeed = CalculateAverage(speedSamples);
        float frictionTorque = currentDamping.Value() * averageSpeed;
        float netTorque = appliedTorque - frictionTorque;

        float inertia = netTorque / *acceleration;

        if (inertia <= 0.0f)
        {
            onInertiaDone(std::nullopt);
            return;
        }

        onInertiaDone(foc::NewtonMeterSecondSquared{ inertia });
    }
}
