#include "core/state_machine/NvmActivity.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    void NvmActivity::Begin()
    {
        ++outstanding;
    }

    void NvmActivity::End()
    {
        really_assert(outstanding != 0);
        --outstanding;
    }

    bool NvmActivity::InFlight() const
    {
        return outstanding != 0;
    }
}
