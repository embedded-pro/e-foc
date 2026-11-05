#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include "core/services/standstill/RotorStandstill.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace application
{
    class CurrentOffsetRefresh
    {
    public:
        using Offsets = std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>>;
        using Standstill = std::optional<std::reference_wrapper<services::RotorStandstill>>;

        CurrentOffsetRefresh(Offsets offsets, Standstill standstill, services::Tracer& tracer);

        bool IsDue() const;
        void MarkStale();
        void Start(const foc::PhaseCurrents& stored, const infra::Function<void(state_machine::OffsetRefreshOutcome)>& onDone);
        bool IsRunning() const;
        void Abort();

    private:
        void Measure();
        void Conclude(const std::optional<foc::PhaseCurrents>& measured);
        bool IsNearStored(const foc::PhaseCurrents& measured) const;

        Offsets offsets;
        Standstill standstill;
        services::Tracer& tracer;
        foc::Ampere driftLimit{ 0.05f };
        foc::PhaseCurrents stored{};
        infra::AutoResetFunction<void(state_machine::OffsetRefreshOutcome)> onDone;
        bool fresh{ false };
        uint32_t run{ 0 };
    };
}
