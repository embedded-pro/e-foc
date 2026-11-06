#pragma once

#include "core/services/fault_history/FaultLog.hpp"
#include <gmock/gmock.h>

namespace services
{
    class FaultLogMock
        : public FaultLog
    {
    public:
        MOCK_METHOD(std::optional<FaultRecord>, OfAge, (std::size_t age), (const, override));
        MOCK_METHOD(void, Clear, (const infra::Function<void(bool stored)>& onDone), (override));
    };
}
