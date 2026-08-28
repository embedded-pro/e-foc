#pragma once

#include "core/platform_abstraction/test_doubles/CanBusAdapterMock.hpp"
#include "core/platform_abstraction/test_doubles/PlatformFactoryMock.hpp"

namespace sil
{
    class PlatformFactoryMock
        : public application::PlatformFactoryMock
    {
    public:
        testing::StrictMock<application::CanBusAdapterMock> canAdapterMock;
    };
}
