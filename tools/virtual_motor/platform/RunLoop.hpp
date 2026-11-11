#pragma once

#include "tools/virtual_motor/platform/Pacer.hpp"
#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstdint>
#include <optional>

namespace virtual_motor
{
    class Stepper
    {
    public:
        virtual void AdvanceOnePeriod() = 0;
        virtual void AdvanceOneMillisecond() = 0;
        virtual void Mute(bool muted) = 0;

    protected:
        ~Stepper() = default;
    };

    class RunLoop
    {
    public:
        RunLoop(Stepper& stepper, const infra::Function<Pacer::TimePoint()>& now);

        void Begin();
        void Pause();
        void Resume();
        bool Paused() const;
        std::optional<port::RefusalReason> Step(port::StepUnit unit, uint16_t count);

        void Service();
        Pacer::TimePoint NextWake() const;
        float RealTimeFactor() const;
        uint32_t Overruns() const;

    private:
        Stepper& stepper;
        infra::Function<Pacer::TimePoint()> now;
        Pacer pacer;
        bool paused{ false };
    };
}
