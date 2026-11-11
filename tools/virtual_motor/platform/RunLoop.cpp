#include "tools/virtual_motor/platform/RunLoop.hpp"

namespace virtual_motor
{
    RunLoop::RunLoop(Stepper& stepper, const infra::Function<Pacer::TimePoint()>& now)
        : stepper(stepper)
        , now(now)
        , pacer{ now, [this]()
            {
                this->stepper.AdvanceOneMillisecond();
            } }
    {}

    void RunLoop::Begin()
    {
        pacer.Begin();
    }

    void RunLoop::Pause()
    {
        if (paused)
            return;

        paused = true;
        pacer.Suspend();
        stepper.Mute(true);
    }

    void RunLoop::Resume()
    {
        if (!paused)
            return;

        paused = false;
        pacer.Resume();
        stepper.Mute(false);
    }

    bool RunLoop::Paused() const
    {
        return paused;
    }

    std::optional<port::RefusalReason> RunLoop::Step(port::StepUnit unit, uint16_t count)
    {
        if (!paused)
            return port::RefusalReason::stepWhileRunning;
        if (unit != port::StepUnit::controlPeriod && unit != port::StepUnit::millisecond)
            return port::RefusalReason::outOfRange;

        for (uint16_t step = 0; step != count; ++step)
        {
            if (unit == port::StepUnit::controlPeriod)
                stepper.AdvanceOnePeriod();
            else
                stepper.AdvanceOneMillisecond();
        }
        return std::nullopt;
    }

    void RunLoop::Service()
    {
        if (!paused)
            pacer.Service();
    }

    Pacer::TimePoint RunLoop::NextWake() const
    {
        if (paused)
            return now() + Pacer::step;
        return pacer.NextDue();
    }

    float RunLoop::RealTimeFactor() const
    {
        return pacer.RealTimeFactor();
    }

    uint32_t RunLoop::Overruns() const
    {
        return pacer.Overruns();
    }
}
