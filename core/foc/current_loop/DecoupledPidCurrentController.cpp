#include "core/foc/current_loop/DecoupledPidCurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"

namespace foc
{
    bool DecoupledPidCurrentController::Configure(const MotorModelParameters& motorParameters)
    {
        const bool pidOk = pid.Configure(motorParameters);
        const bool decouplingOk = decoupling.Configure(motorParameters);
        return pidOk && decouplingOk;
    }

    bool DecoupledPidCurrentController::SetTunings(const CurrentLoopTunings& tunings)
    {
        return pid.SetTunings(tunings);
    }

    void DecoupledPidCurrentController::Reset()
    {
        pid.Reset();
    }

}
