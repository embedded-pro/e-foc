#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/interfaces/AdcMultiChannel.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <concepts>
#include <utility>

namespace application
{
    class AdcPhaseCurrentMeasurement
    {
    public:
        virtual ~AdcPhaseCurrentMeasurement() = default;

        virtual void Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone) = 0;
        virtual void Stop() = 0;
    };

    template<typename Impl>
    requires std::derived_from<Impl, hal::AdcMultiChannel>
    class AdcPhaseCurrentMeasurementImpl
        : public AdcPhaseCurrentMeasurement
    {
    public:
        template<typename... Args>
        explicit AdcPhaseCurrentMeasurementImpl(float slope, float offset, Args&&... args);

        void Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone) override;
        void Stop() override;

    private:
        static constexpr std::size_t phaseCount{ 3 };

        Impl adc;
        float slope = 0.0f;
        float offset = 0.0f;
        infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)> onMeasurementDone;
    };

    template<typename Impl>
    requires std::derived_from<Impl, hal::AdcMultiChannel>
    template<typename... Args>
    AdcPhaseCurrentMeasurementImpl<Impl>::AdcPhaseCurrentMeasurementImpl(float slope, float offset, Args&&... args)
        : adc(std::forward<Args>(args)...)
        , slope(slope)
        , offset(offset)
    {
    }

    template<typename Impl>
    requires std::derived_from<Impl, hal::AdcMultiChannel>
    OPTIMIZE_FOR_SPEED void AdcPhaseCurrentMeasurementImpl<Impl>::Measure(const infra::Function<void(foc::Ampere phaseA, foc::Ampere phaseB, foc::Ampere phaseC)>& onDone)
    {
        onMeasurementDone = onDone;
        adc.Measure([this](auto samples)
            {
                if (samples.size() < phaseCount) [[unlikely]]
                    return;

                const float s = slope;
                const float o = offset;
                onMeasurementDone(
                    foc::Ampere{ static_cast<float>(samples[0]) * s + o },
                    foc::Ampere{ static_cast<float>(samples[1]) * s + o },
                    foc::Ampere{ static_cast<float>(samples[2]) * s + o });
            });
    }

    template<typename Impl>
    requires std::derived_from<Impl, hal::AdcMultiChannel>
    void AdcPhaseCurrentMeasurementImpl<Impl>::Stop()
    {
        adc.Stop();
    }
}
