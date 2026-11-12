#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "infra/util/Function.hpp"

namespace foc
{
    class LowPriorityInterrupt
    {
    protected:
        ~LowPriorityInterrupt() = default;

    public:
        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
        virtual void Unregister() = 0;
    };

    class ZeroVectorSlopeSink
    {
    protected:
        ~ZeroVectorSlopeSink() = default;

    public:
        virtual void ZeroVectorSlopes(const PhaseCurrentSlopes& slopes) = 0;
    };

    class Controllable
    {
    protected:
        ~Controllable() = default;

    public:
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };

    class PhaseCurrentsObservable
    {
    protected:
        ~PhaseCurrentsObservable() = default;

    public:
        // With the phases come the q-axis current the loop measured from them and the mechanical angle it used.
        virtual void RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& phases, Ampere quadratureCurrent, Radians mechanicalAngle)>& observer) = 0;
        virtual void UnregisterPhaseCurrentsObserver() = 0;
    };
}
