#include "core/foc/model/PhaseCurrentSensor.hpp"
#include "infra/util/Function.hpp"

namespace foc
{
    void PhaseCurrentSensor::SetNoise(const CurrentNoiseConfig& config)
    {
        noise = config;
    }

    void PhaseCurrentSensor::SetChain(const CurrentSenseConfig& config)
    {
        chain = config;
    }

    void PhaseCurrentSensor::Seed(uint32_t seed)
    {
        engine.seed(seed);
    }

    void PhaseCurrentSensor::Measure(const foc::PhasePwmDutyCycles& duties, const foc::PhaseCurrents& actual, const PowerStageConfig& powerStage, float period)
    {
        if (!CurrentSenseEnabled(chain))
        {
            last = { foc::Ampere{ actual.a.Value() + noise.biasAmpereA + SampleNoise() },
                foc::Ampere{ actual.b.Value() + noise.biasAmpereB + SampleNoise() },
                foc::Ampere{ actual.c.Value() + noise.biasAmpereC + SampleNoise() } };
            return;
        }

        const infra::Function<float()> conversionNoise{ [this]()
            {
                return SampleNoise();
            } };
        const auto read = [this, period, &powerStage, &conversionNoise](uint32_t step, hal::DutyCycle duty, foc::Ampere current, float offset)
        {
            const ShuntSample sample{ LowSideConduction(powerStage, foc::DutyFraction(duty), current.Value(), period), current.Value(), offset };
            return foc::Ampere{ ShuntReading(chain, sample, step, conversionNoise) };
        };

        last = { read(0, duties.a, actual.a, noise.biasAmpereA), read(1, duties.b, actual.b, noise.biasAmpereB), read(2, duties.c, actual.c, noise.biasAmpereC) };
    }

    foc::PhaseCurrents PhaseCurrentSensor::Last() const
    {
        return last;
    }

    float PhaseCurrentSensor::SampleNoise()
    {
        return noise.sigmaAmpere * distribution(engine);
    }
}
