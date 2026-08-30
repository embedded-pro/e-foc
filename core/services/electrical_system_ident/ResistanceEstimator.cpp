#include "core/services/electrical_system_ident/ResistanceEstimator.hpp"
#include <numeric>

namespace
{
    const hal::Hertz samplingFrequency{ 10000 };

    foc::PhasePwmDutyCycles NeutralDuties(uint8_t neutral)
    {
        return foc::PhasePwmDutyCycles{
            hal::Percent{ neutral }, hal::Percent{ neutral }, hal::Percent{ neutral }
        };
    }

    float AverageAndRemoveFront(infra::BoundedDeque<float>& deque)
    {
        float sum = 0.0f;
        for (const auto& s : deque)
            sum += s;
        const float avg = sum / static_cast<float>(deque.size());
        deque.pop_front();
        return avg;
    }

    float GetSteadyStateCurrent(const infra::BoundedVector<float>& samples)
    {
        const auto lastQuarter = static_cast<std::size_t>(static_cast<float>(samples.size()) * 0.9f);
        return std::accumulate(samples.begin() + lastQuarter, samples.end(), 0.0f) / static_cast<float>(samples.size() - lastQuarter);
    }
}

namespace services
{
    ResistanceEstimator::ResistanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc)
        : driver(driver)
        , vdc(vdc)
    {}

    void ResistanceEstimator::Start(const Config& config, const infra::Function<void(Result)>& onDone)
    {
        activeConfig = config;
        this->onDone = onDone;
        currentSamples.clear();
        filteredSamples.clear();

        driver.PhaseCurrentsReady(samplingFrequency, [](auto) {});
        driver.ThreePhasePwmOutput(NeutralDuties(neutralDuty));

        settleTimer.Start(config.settleTime, [this]()
            {
                driver.PhaseCurrentsReady(samplingFrequency, [this](auto currents)
                    {
                        currentSamples.push_back(currents.a.Value());

                        if (currentSamples.full())
                            filteredSamples.push_back(AverageAndRemoveFront(currentSamples));

                        if (filteredSamples.full())
                            OnMeasurementComplete();
                    });

                driver.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{
                    hal::Percent{ activeConfig.testVoltagePercent.Value() },
                    hal::Percent{ neutralDuty },
                    hal::Percent{ neutralDuty } });
            });
    }

    void ResistanceEstimator::OnMeasurementComplete()
    {
        driver.Stop();

        const float steadyStateCurrent = GetSteadyStateCurrent(filteredSamples);

        if (steadyStateCurrent <= 0.0f)
        {
            onDone(Result{});
            return;
        }

        const float appliedDuty = static_cast<float>(activeConfig.testVoltagePercent.Value() - neutralDuty);
        const float terminalVoltage = appliedDuty * vdc.Value() / 100.0f;
        const float terminalFactor = activeConfig.windingConfig == WindingConfiguration::Delta
                                         ? deltaTerminalFactor
                                         : wyeTerminalFactor;
        const float phaseResistance = terminalVoltage / steadyStateCurrent / terminalFactor;

        filteredSamples.clear();
        onDone(Result{ foc::Ohm{ phaseResistance } });
    }
}
