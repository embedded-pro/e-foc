#include "core/services/mechanical_system_ident/TwoLevelSpeedProfile.hpp"
#include "numerical/math/Math.hpp"

namespace services
{
    void TwoLevelSpeedProfile::Start(const Levels& levels, infra::Duration dwellTime)
    {
        this->levels = levels;
        atDwellLevel = true;
        levelSpeed = levels.dwellSpeed;
        commandedSpeed = 0.0f;
        levelTimer.Start(dwellTime, [this]()
            {
                SwitchLevel();
            });
    }

    void TwoLevelSpeedProfile::Stop()
    {
        levelTimer.Cancel();
    }

    float TwoLevelSpeedProfile::Step(float periodSeconds)
    {
        const auto step = levels.slewRate * periodSeconds;
        const auto remaining = levelSpeed - commandedSpeed;

        commandedSpeed = math::Abs(remaining) <= step ? levelSpeed : commandedSpeed + math::Copysign(step, remaining);
        return commandedSpeed;
    }

    void TwoLevelSpeedProfile::SwitchLevel()
    {
        atDwellLevel = !atDwellLevel;
        levelSpeed = atDwellLevel ? levels.dwellSpeed : levels.targetSpeed;
    }
}
