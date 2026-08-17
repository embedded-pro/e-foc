#include "core/foc/current_loop/DecoupledPidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    void DecoupledPidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        pid.Configure(motorParameters);
        decoupling.Configure(motorParameters);
    }

    void DecoupledPidCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        pid.SetTunings(tunings);
    }

    void DecoupledPidCurrentController::Reset()
    {
        pid.Reset();
    }

}
