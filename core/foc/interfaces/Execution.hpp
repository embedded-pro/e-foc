#pragma once

#include "infra/util/Function.hpp"

namespace foc
{
    class LowPriorityInterrupt
    {
    public:
        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
        virtual void Unregister() = 0;
    };

    class Controllable
    {
    public:
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };
}
