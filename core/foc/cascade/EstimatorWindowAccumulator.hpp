#pragma once

#include "core/foc/cascade/MechanicalWindowAccumulator.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    struct EstimatorSnapshot
    {
        float meanIq{ 0.0f };
        float meanId{ 0.0f };
        float idAtEnd{ 0.0f };
        float meanNormalizedVd{ 0.0f };
        float meanElectricalSpeedTimesIq{ 0.0f };
        MechanicalWindow mechanical{};
    };

    class EstimatorWindowAccumulator
    {
    public:
        EstimatorWindowAccumulator(hal::Hertz tickFrequency, uint32_t ticksPerWindow, uint32_t dutyUpdateDelayPeriods = 0);

        void Restart();

        ALWAYS_INLINE_HOT void Accumulate(RotatingFrame measured, RotatingFrame normalizedVoltage, float mechanicalAngle, float polePairs, float coggingCurrent)
        {
            const auto mechanicalRotation = rotationPrimed ? detail::PositionWithWrapAround(mechanicalAngle - previousMechanicalAngle) : 0.0f;
            const auto electricalRotation = mechanicalRotation * polePairs;
            previousMechanicalAngle = mechanicalAngle;
            rotationPrimed = true;

            mechanical.Accumulate(measured.q - coggingCurrent, mechanicalRotation);

            const auto meanIq = 0.5f * (measured.q + previousIq);

            const auto& applied = dutyLate ? earlierNormalizedVoltage : previousNormalizedVoltage;

            iqSum += meanIq;
            idSum += 0.5f * (measured.d + previousId);
            vdSum += applied.d + 0.5f * electricalRotation * applied.q;
            rotationTimesIqSum += electricalRotation * meanIq;

            previousIq = measured.q;
            previousId = measured.d;
            earlierNormalizedVoltage = previousNormalizedVoltage;
            previousNormalizedVoltage = normalizedVoltage;
            ++ticks;
        }

        ALWAYS_INLINE_HOT EstimatorSnapshot Close()
        {
            const auto inverseTicks = ticks != 0 ? 1.0f / static_cast<float>(ticks) : 0.0f;
            const EstimatorSnapshot snapshot{ iqSum * inverseTicks, idSum * inverseTicks, previousId, vdSum * inverseTicks, rotationTimesIqSum * inverseTicks * tickFrequency, mechanical.Close() };

            iqSum = 0.0f;
            idSum = 0.0f;
            vdSum = 0.0f;
            rotationTimesIqSum = 0.0f;
            ticks = 0;

            return snapshot;
        }

    private:
        float iqSum{ 0.0f };
        float idSum{ 0.0f };
        float vdSum{ 0.0f };
        float rotationTimesIqSum{ 0.0f };
        float previousIq{ 0.0f };
        float previousId{ 0.0f };
        RotatingFrame previousNormalizedVoltage{};
        RotatingFrame earlierNormalizedVoltage{};
        float previousMechanicalAngle{ 0.0f };
        bool rotationPrimed{ false };
        float tickFrequency{ 0.0f };
        uint32_t ticksPerWindow;
        bool dutyLate;
        MechanicalWindowAccumulator mechanical;
        uint32_t ticks{ 0 };
    };
}
