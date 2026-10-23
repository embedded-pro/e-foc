#pragma once

#include "infra/util/Function.hpp"

namespace services
{
    class RotorStandstill
    {
    protected:
        ~RotorStandstill() = default;

    public:
        virtual void Await(const infra::Function<void(bool atRest)>& onDone) = 0;
        virtual void Abort() = 0;
    };
}
