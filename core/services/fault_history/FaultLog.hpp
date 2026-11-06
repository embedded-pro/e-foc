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
        virtual std::optional<FaultRecord> OfAge(std::size_t age) const = 0;
        virtual void Clear(const infra::Function<void(bool stored)>& onDone) = 0;

    protected:
        ~FaultLog() = default;
    };
}
