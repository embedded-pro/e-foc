#pragma once

#include "infra/util/Function.hpp"

namespace foc
{
    class LowPriorityInterrupt
    {
    public:
        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
        // After this returns, no already queued nor future trigger may invoke the handler
        virtual void Unregister() = 0;
    };

    class Controllable
    {
    public:
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };
}
