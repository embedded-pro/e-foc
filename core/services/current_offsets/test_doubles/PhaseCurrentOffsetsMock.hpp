#pragma once

#include "core/services/current_offsets/PhaseCurrentOffsets.hpp"
#include <gmock/gmock.h>

namespace services
{
    class PhaseCurrentOffsetsMock
        : public PhaseCurrentOffsets
    {
    public:
        MOCK_METHOD(void, Measure, (const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone), (override));
        MOCK_METHOD(void, Apply, (const foc::PhaseCurrents& offsets), (override));
        MOCK_METHOD(bool, IsRunning, (), (const, override));
        MOCK_METHOD(void, Abort, (), (override));
    };
}
