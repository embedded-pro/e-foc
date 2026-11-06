#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/BoundedString.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace application
{
    enum class ResetCause : uint8_t;
}

namespace services
{
    struct StartupBanner
    {
        infra::BoundedConstString::WithStorage<32> targetName;
        foc::Volts vdc;
        hal::Hertz systemClock;
        application::ResetCause resetCause{};
        infra::BoundedConstString faultStatus;
        infra::BoundedConstString version;
        infra::BoundedConstString revision;
    };

    void TraceStartupBanner(services::Tracer& tracer, const StartupBanner& banner);
}
