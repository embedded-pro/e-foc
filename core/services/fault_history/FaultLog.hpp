#pragma once

#include "core/services/non_volatile_memory/FaultHistoryData.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <optional>

namespace services
{
    class FaultLog
    {
    public:
        using LoadedAction = infra::Function<void(), 40>;

        virtual std::optional<FaultRecord> OfAge(std::size_t age) const = 0;
        virtual void WhenLoaded(const LoadedAction& action) = 0;
        virtual void Clear(const infra::Function<void(bool stored)>& onDone) = 0;

    protected:
        ~FaultLog() = default;
    };
}
