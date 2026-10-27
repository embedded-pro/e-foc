#include "core/foc/cascade/SpeedDifferentiator.hpp"
#include "core/foc/math/AngleWrap.hpp"

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
        // The window interrupt preempts this reader, so a changed index means the angle may belong to a newer window
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

        lastSpeed = detail::PositionWithWrapAround(angle - previousAngle) / (static_cast<float>(windows) * samplePeriod);
        previousAngle = angle;
        previousIndex = index;
        return lastSpeed;
    }
}
