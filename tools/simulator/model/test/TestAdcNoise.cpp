#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace
{
    class TestAdcNoise
        : public ::testing::Test
    {
    protected:
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        simulator::ThreePhaseMotorModel model{
            simulator::JK42BLS01_X038ED::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 100000 },
            std::optional<std::size_t>{}
        };
    };

    class CurrentCollector
        : public simulator::ThreePhaseMotorModelObserver
    {
    public:
        explicit CurrentCollector(simulator::ThreePhaseMotorModel& model)
            : ThreePhaseMotorModelObserver(model)
        {}

        void Started() override
        {}

        void StatorVoltages(foc::ThreePhase, foc::TwoPhase) override
        {}

        void Finished() override
        {}

        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currents, foc::Radians, foc::RadiansPerSecond) override
        {
            samples.push_back(currents.a.Value());
        }

        std::vector<float> samples;
    };

    void DriveModelCycles(simulator::ThreePhaseMotorModel& model, int cycles)
    {
        const foc::PhasePwmDutyCycles duty{
            hal::Percent{ 60 },
            hal::Percent{ 50 },
            hal::Percent{ 40 }
        };
        for (int i = 0; i < cycles; ++i)
            model.StepForTest(duty);
    }
}

TEST_F(TestAdcNoise, zero_sigma_produces_finite_currents)
{
    model.SetAdcNoise(simulator::ThreePhaseMotorModel::NoiseConfig{});

    CurrentCollector collector{ model };
    DriveModelCycles(model, 200);

    ASSERT_FALSE(collector.samples.empty());
    for (auto s : collector.samples)
        EXPECT_TRUE(std::isfinite(s));
}

TEST_F(TestAdcNoise, nonzero_sigma_increases_variance_over_zero_sigma_baseline)
{
    constexpr int cycles = 5000;

    CurrentCollector baseCollector{ model };
    DriveModelCycles(model, cycles);
    const auto baselineSize = static_cast<float>(baseCollector.samples.size());

    float baseSum2 = 0.0f;
    float baseSum = 0.0f;
    for (auto s : baseCollector.samples)
    {
        baseSum += s;
        baseSum2 += s * s;
    }
    const float baseMean = baseSum / baselineSize;
    const float baseVar = baseSum2 / baselineSize - baseMean * baseMean;

    simulator::ThreePhaseMotorModel model2{
        simulator::JK42BLS01_X038ED::parameters,
        foc::Volts{ 24.0f },
        hal::Hertz{ 100000 },
        std::optional<std::size_t>{}
    };
    constexpr float sigma = 0.1f;
    model2.SetAdcNoise(simulator::ThreePhaseMotorModel::NoiseConfig{ .sigmaAmpere = sigma });

    CurrentCollector noisyCollector{ model2 };
    const foc::PhasePwmDutyCycles duty{ hal::Percent{ 60 }, hal::Percent{ 50 }, hal::Percent{ 40 } };
    for (int i = 0; i < cycles; ++i)
        model2.StepForTest(duty);

    const auto noisySize = static_cast<float>(noisyCollector.samples.size());
    float noisySum2 = 0.0f;
    float noisySum = 0.0f;
    for (auto s : noisyCollector.samples)
    {
        noisySum += s;
        noisySum2 += s * s;
    }
    const float noisyMean = noisySum / noisySize;
    const float noisyVar = noisySum2 / noisySize - noisyMean * noisyMean;

    EXPECT_GT(noisyVar, baseVar + (sigma * sigma) * 0.5f)
        << "Noisy channel should have clearly higher variance than pristine channel";
}
