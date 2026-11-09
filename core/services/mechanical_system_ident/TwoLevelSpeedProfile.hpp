#pragma once

#include "infra/timer/Timer.hpp"

namespace services
{
    class TwoLevelSpeedProfile
    {
    public:
        struct Levels
        {
            float dwellSpeed{ 0.0f };
            float targetSpeed{ 0.0f };
            float slewRate{ 0.0f };
        };

        void Start(const Levels& levels, infra::Duration dwellTime);
        void Stop();
        float Step(float periodSeconds);

    private:
        void SwitchLevel();

        Levels levels;
        bool atDwellLevel{ true };
        float levelSpeed{ 0.0f };
        float commandedSpeed{ 0.0f };
        infra::TimerRepeating levelTimer;
    };
}
