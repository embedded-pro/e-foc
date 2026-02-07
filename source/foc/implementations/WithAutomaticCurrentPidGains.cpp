#include "source/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "source/foc/implementations/SpeedControllerImpl.hpp"
#include "source/foc/implementations/TorqueControllerImpl.hpp"

namespace foc
{
    template class WithAutomaticCurrentPidGains<TorqueControllerImpl>;
    template class WithAutomaticCurrentPidGains<SpeedControllerImpl>;
}
