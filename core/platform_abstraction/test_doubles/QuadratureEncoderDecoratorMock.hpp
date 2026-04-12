#pragma once

#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include <gmock/gmock.h>

namespace application
{
    class QuadratureEncoderDecoratorMock
        : public QuadratureEncoderDecorator
    {
    public:
        MOCK_METHOD(foc::Radians, Read, (), (override));
    };
}
