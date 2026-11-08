#include "core/foc/cascade/SpeedDifferentiator.hpp"
#include "numerical/math/AngleUtilities.hpp"

namespace foc
{
    SpeedDifferentiator::SpeedDifferentiator(hal::Hertz outerLoopFrequency)
        : samplePeriod{ 1.0f / static_cast<float>(outerLoopFrequency.Value()) }
    {}

    void SpeedDifferentiator::Restart()
    {
        currentAngle = 0.0f;
        windowAngle = 0.0f;
        previousAngle = 0.0f;
        lastSpeed = 0.0f;
        previousAngleValid = false;
    }

    float SpeedDifferentiator::Measure()
    {
        uint32_t index{};
        float angle{};
        do
        {
            index = windowIndex;
            angle = windowAngle;
        } while (index != windowIndex);

        if (!previousAngleValid)
        {
            previousAngle = angle;
            previousIndex = index;
            previousAngleValid = true;
            return 0.0f;
        }

        const auto windows = index - previousIndex;
        if (windows == 0)
            return lastSpeed;

        lastSpeed = math::WrapToPi(angle - previousAngle) / (static_cast<float>(windows) * samplePeriod);
        previousAngle = angle;
        previousIndex = index;
        return lastSpeed;
    }
}
