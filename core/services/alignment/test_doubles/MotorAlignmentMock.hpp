#pragma once

#include "core/services/alignment/MotorAlignment.hpp"
#include <gmock/gmock.h>

namespace services
{
    class MotorAlignmentMock
        : public MotorAlignment
    {
    public:
        MOCK_METHOD(void, ForceAlignment,
            (std::size_t polePairs, const AlignmentConfig& config,
                const infra::Function<void(std::optional<foc::Radians>)>& onDone),
            (override));
    };
}
