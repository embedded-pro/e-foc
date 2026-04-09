#pragma once

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include <type_traits>
#include <utility>

namespace application
{
    class AdcPhaseCurrentMeasurement
    {
    public:
        virtual void Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone) = 0;
        virtual void Stop() = 0;
    };

    template<typename Impl, typename = std::enable_if_t<std::is_base_of_v<hal::AdcMultiChannel, Impl>>>
    class AdcPhaseCurrentMeasurementImpl
        : public AdcPhaseCurrentMeasurement
    {
    public:
        template<typename... Args>
        explicit AdcPhaseCurrentMeasurementImpl(float slope, float offset, Args&&... args);

        void Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone) override;
        void Stop() override;

    private:
        Impl adc;
        float slope = 0.0f;
        float offset = 0.0f;
        infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)> onMeasurementDone;
    };

    // Implementation

    template<typename Impl, typename Enable>
    template<typename... Args>
    AdcPhaseCurrentMeasurementImpl<Impl, Enable>::AdcPhaseCurrentMeasurementImpl(float slope, float offset, Args&&... args)
        : adc(std::forward<Args>(args)...)
        , slope(slope)
        , offset(offset)
    {
    }

    template<typename Impl, typename Enable>
    void AdcPhaseCurrentMeasurementImpl<Impl, Enable>::Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone)
    {
        onMeasurementDone = onDone;
        adc.Measure([this](auto samples)
            {
                const float s = slope;
                const float o = offset;
                onMeasurementDone(
                    foc::Ampere{ static_cast<float>(samples[0]) * s + o },
                    foc::Ampere{ static_cast<float>(samples[1]) * s + o },
                    foc::Ampere{ static_cast<float>(samples[2]) * s + o });
            });
    }

    template<typename Impl, typename Enable>
    void AdcPhaseCurrentMeasurementImpl<Impl, Enable>::Stop()
    {
        adc.Stop();
    }
}
