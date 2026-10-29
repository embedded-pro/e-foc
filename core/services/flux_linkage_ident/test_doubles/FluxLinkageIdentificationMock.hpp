#pragma once

#include "core/services/flux_linkage_ident/FluxLinkageIdentification.hpp"
#include <gmock/gmock.h>

namespace services
{
    class FluxLinkageIdentificationMock
        : public FluxLinkageIdentification
    {
    public:
        MOCK_METHOD(void, Identify, (std::size_t polePairs, foc::Ohm resistance, const Config& config, const infra::Function<void(const Result& result)>& onDone), (override));
        MOCK_METHOD(bool, IsRunning, (), (const, override));
        MOCK_METHOD(void, Abort, (), (override));
    };
}
