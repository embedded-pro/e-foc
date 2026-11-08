#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cstdint>

namespace foc
{
    class SpeedDifferentiator
    {
    public:
        explicit SpeedDifferentiator(hal::Hertz outerLoopFrequency);

        void Restart();
        void SetFilterTimeConstant(float seconds);

        ALWAYS_INLINE_HOT void Track(float mechanicalAngle)
        {
            currentAngle = mechanicalAngle;
        }

        ALWAYS_INLINE_HOT float CurrentAngle() const
        {
            return currentAngle;
        }

        ALWAYS_INLINE_HOT void CloseWindow()
        {
            windowAngle = currentAngle;
            windowIndex = windowIndex + 1;
        }

        float Measure();

        ALWAYS_INLINE_HOT float FilteredSpeed() const
        {
            return filteredSpeed;
        }

    private:
        float samplePeriod;
        volatile float currentAngle{ 0.0f };
        volatile float windowAngle{ 0.0f };
        volatile uint32_t windowIndex{ 0 };
        float previousAngle{ 0.0f };
        uint32_t previousIndex{ 0 };
        float lastSpeed{ 0.0f };
        float filteredSpeed{ 0.0f };
        filters::passive::ExponentialMovingAverage<float> filter{ 1.0f };
        bool previousAngleValid{ false };
    };
}
