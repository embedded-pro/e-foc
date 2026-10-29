#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"

namespace services
{
    class ConfirmedPhaseCurrentOffsets
        : public PhaseCurrentOffsets
    {
    public:
        struct Config
        {
            foc::Ampere tolerance{ 0.01f };
        };

        explicit ConfirmedPhaseCurrentOffsets(PhaseCurrentOffsets& measurement);
        ConfirmedPhaseCurrentOffsets(PhaseCurrentOffsets& measurement, const Config& config);

        void Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone) override;
        void Apply(const foc::PhaseCurrents& offsets) override;
        foc::Ampere Noise() const override;
        bool IsRunning() const override;
        void Abort() override;

    private:
        struct Window
        {
            foc::PhaseCurrents offsets;
            foc::Ampere noise;
        };

        void MeasureWindow();
        void OnWindow(std::optional<foc::PhaseCurrents> offsets);
        bool Agree(const Window& first, const Window& second) const;
        void Confirm(const Window& first, const Window& second);

        PhaseCurrentOffsets& measurement;
        Config config;
        infra::AutoResetFunction<void(std::optional<foc::PhaseCurrents>)> onMeasured;
        infra::BoundedVector<Window>::WithMaxSize<3> windows;
        foc::Ampere noise{ 0.0f };
    };
}
