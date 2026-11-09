#include "core/supervision/PeakCurrent.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>

namespace supervision
{
    void PeakCurrent::Attach(const infra::Function<const state_machine::FocStateMachineBase&()>& activeDrive)
    {
        this->activeDrive = activeDrive;
        sampleTimer.Start(samplePeriod, [this]()
            {
                Sample();
            });
    }

    foc::Ampere PeakCurrent::Peak() const
    {
        const auto largest = std::ranges::max_element(samples, [](float left, float right)
            {
                return math::Abs(left) < math::Abs(right);
            });
        return foc::Ampere{ *largest };
    }

    void PeakCurrent::Sample()
    {
        samples[next] = activeDrive().ObserveMotion().measuredTorqueCurrent.Value();
        next = (next + 1) % window;
    }
}
