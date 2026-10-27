#include "core/foc/cascade/EstimatorWindowAccumulator.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace foc
{
    EstimatorWindowAccumulator::EstimatorWindowAccumulator(hal::Hertz tickFrequency, uint32_t ticksPerWindow, uint32_t dutyUpdateDelayPeriods)
        : tickFrequency{ static_cast<float>(tickFrequency.Value()) }
        , ticksPerWindow{ ticksPerWindow }
        , dutyLate{ dutyUpdateDelayPeriods != 0 }
        , mechanical{ ticksPerWindow }
    {
        really_assert(dutyUpdateDelayPeriods <= 1);
    }

    void EstimatorWindowAccumulator::Restart()
    {
        *this = EstimatorWindowAccumulator{ hal::Hertz{ static_cast<uint32_t>(tickFrequency) }, ticksPerWindow, dutyLate ? 1u : 0u };
    }
}
