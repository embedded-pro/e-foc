#include "core/services/current_offsets/ConfirmedPhaseCurrentOffsets.hpp"
#include <cmath>

namespace services
{
    namespace
    {
        foc::Ampere Mean(foc::Ampere first, foc::Ampere second)
        {
            return foc::Ampere{ 0.5f * (first.Value() + second.Value()) };
        }
    }

    ConfirmedPhaseCurrentOffsets::ConfirmedPhaseCurrentOffsets(PhaseCurrentOffsets& measurement)
        : ConfirmedPhaseCurrentOffsets(measurement, Config{})
    {}

    ConfirmedPhaseCurrentOffsets::ConfirmedPhaseCurrentOffsets(PhaseCurrentOffsets& measurement, const Config& config)
        : measurement(measurement)
        , config(config)
    {}

    void ConfirmedPhaseCurrentOffsets::Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone)
    {
        windows.clear();
        noise = foc::Ampere{ 0.0f };
        onMeasured = onDone;
        MeasureWindow();
    }

    void ConfirmedPhaseCurrentOffsets::Apply(const foc::PhaseCurrents& offsets)
    {
        measurement.Apply(offsets);
    }

    foc::Ampere ConfirmedPhaseCurrentOffsets::Noise() const
    {
        return noise;
    }

    bool ConfirmedPhaseCurrentOffsets::IsRunning() const
    {
        return static_cast<bool>(onMeasured);
    }

    void ConfirmedPhaseCurrentOffsets::Abort()
    {
        if (!onMeasured)
            return;

        onMeasured = nullptr;
        measurement.Abort();
    }

    void ConfirmedPhaseCurrentOffsets::MeasureWindow()
    {
        measurement.Measure([this](std::optional<foc::PhaseCurrents> offsets)
            {
                OnWindow(offsets);
            });
    }

    void ConfirmedPhaseCurrentOffsets::OnWindow(std::optional<foc::PhaseCurrents> offsets)
    {
        if (!onMeasured)
            return;

        if (!offsets.has_value())
        {
            onMeasured(std::nullopt);
            return;
        }

        windows.push_back(Window{ *offsets, measurement.Noise() });

        for (auto earlier = windows.begin(); earlier + 1 != windows.end(); ++earlier)
            if (Agree(*earlier, windows.back()))
            {
                Confirm(*earlier, windows.back());
                return;
            }

        if (windows.full())
            onMeasured(std::nullopt);
        else
            MeasureWindow();
    }

    bool ConfirmedPhaseCurrentOffsets::Agree(const Window& first, const Window& second) const
    {
        const auto close = [this](foc::Ampere a, foc::Ampere b)
        {
            return std::fabs(a.Value() - b.Value()) <= config.tolerance.Value();
        };

        return close(first.offsets.a, second.offsets.a) && close(first.offsets.b, second.offsets.b) && close(first.offsets.c, second.offsets.c);
    }

    void ConfirmedPhaseCurrentOffsets::Confirm(const Window& first, const Window& second)
    {
        noise = Mean(first.noise, second.noise);
        onMeasured(foc::PhaseCurrents{ Mean(first.offsets.a, second.offsets.a), Mean(first.offsets.b, second.offsets.b), Mean(first.offsets.c, second.offsets.c) });
    }
}
