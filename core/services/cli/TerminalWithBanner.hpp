#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "core/foc/interfaces/Units.hpp"

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
        };

        TerminalWithBanner(infra::BoundedVector<Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer, const Banner& banner);
    };
}
