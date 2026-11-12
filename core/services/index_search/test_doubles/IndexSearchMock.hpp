#pragma once

#include "core/services/index_search/IndexSearch.hpp"
#include <gmock/gmock.h>

namespace services
{
    class IndexSearchMock
        : public IndexSearch
    {
    public:
        MOCK_METHOD(bool, Available, (), (const, override));
        MOCK_METHOD(void, Search, (const Config& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone), (override));
        MOCK_METHOD(void, Abort, (), (override));
        MOCK_METHOD(bool, IsRunning, (), (const, override));
    };
}
