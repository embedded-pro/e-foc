#include "core/services/bus_voltage/BusVoltageMonitor.hpp"
#include <numbers>

namespace services
{
    namespace
    {
        float PollAlpha()
        {
            constexpr float pollRateHz = 1.0f / std::chrono::duration<float>(BusVoltageMonitor::pollPeriod).count();
            constexpr float cutoffHz = 1.0f / (2.0f * std::numbers::pi_v<float> * BusVoltageMonitor::timeConstantSeconds);

            return filters::passive::ExponentialMovingAverage<float>::AlphaFromCutoff(cutoffHz, pollRateHz);
        }
    }

    BusVoltageMonitor::BusVoltageMonitor(const infra::Function<foc::Volts()>& measure, const infra::Function<void(foc::Volts)>& onMeasured)
        : measure(measure)
        , onMeasured(onMeasured)
        , filter(PollAlpha())
    {}

    void BusVoltageMonitor::Start()
    {
        primed = false;
        timer.Start(pollPeriod, [this]()
            {
                Poll();
            });
    }

    void BusVoltageMonitor::Stop()
    {
        timer.Cancel();
    }

    void BusVoltageMonitor::Poll()
    {
        const auto measured = measure().Value();
        auto filtered = measured;

        if (primed)
            filtered = filter.Filter(measured);
        else
        {
            filter.Reset(measured);
            primed = true;
        }

        onMeasured(foc::Volts{ filtered });
    }
}
