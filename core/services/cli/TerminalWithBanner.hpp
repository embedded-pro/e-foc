#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/ResetCause.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/BoundedString.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalWithBanner
        : public TerminalWithStorage
    {
    public:
        template<std::size_t Max>
        using WithMaxSize = infra::WithStorage<TerminalWithBanner, infra::BoundedVector<Command>::WithMaxSize<Max>>;

        struct Banner
        {
            infra::BoundedConstString::WithStorage<32> targetName;
            foc::Volts vdc;
            hal::Hertz systemClock;
            application::ResetCause resetCause{ application::ResetCause::powerUp };
            infra::BoundedConstString faultStatus;
        };

        TerminalWithBanner(infra::BoundedVector<Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer, const Banner& banner);
    };
}
