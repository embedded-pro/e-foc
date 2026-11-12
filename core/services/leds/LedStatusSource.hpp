#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include <cstdint>

namespace services
{
    enum class LedLifecycle : uint8_t
    {
        idle,
        calibrating,
        ready,
        enabled,
        fault
    };

    struct LedStatus
    {
        LedLifecycle lifecycle{ LedLifecycle::idle };
        state_machine::FaultCode fault{ state_machine::FaultCode::none };
    };

    class LedStatusSource
    {
    protected:
        ~LedStatusSource() = default;

    public:
        virtual LedStatus Status() const = 0;
    };
}
