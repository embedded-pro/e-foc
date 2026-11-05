#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"
#include <chrono>

namespace services
{
    class BusVoltageMonitor
    {
    public:
        static constexpr std::chrono::milliseconds pollPeriod{ 10 };
        static constexpr float timeConstantSeconds{ 0.05f };

        BusVoltageMonitor(const infra::Function<foc::Volts()>& measure, const infra::Function<void(foc::Volts)>& onMeasured);

        void Start();
        void Stop();

    private:
        void Poll();

        infra::Function<foc::Volts()> measure;
        infra::Function<void(foc::Volts)> onMeasured;
        filters::passive::ExponentialMovingAverage<float> filter;
        bool primed{ false };
        infra::TimerRepeating timer;
    };
}
