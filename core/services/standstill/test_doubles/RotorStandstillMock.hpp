#pragma once

#include "core/services/standstill/RotorStandstill.hpp"
#include <gmock/gmock.h>

namespace services
{
    class RotorStandstillMock
        : public RotorStandstill
    {
    public:
        MOCK_METHOD(void, Await, (const infra::Function<void(bool atRest)>& onDone), (override));
        MOCK_METHOD(void, Abort, (), (override));
    };
}
