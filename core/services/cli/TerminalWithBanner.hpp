#pragma once

#include "core/services/cli/StartupBanner.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    class TerminalWithBanner
        : public TerminalWithStorage
    {
    public:
        template<std::size_t Max>
        using WithMaxSize = infra::WithStorage<TerminalWithBanner, infra::BoundedVector<Command>::WithMaxSize<Max>>;

        using Banner = StartupBanner;

        TerminalWithBanner(infra::BoundedVector<Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer, const Banner& banner);
    };
}
