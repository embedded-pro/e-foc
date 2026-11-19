#include "tools/virtual_motor/platform/DrivenShaft.hpp"
#include <algorithm>

namespace virtual_motor
{
    DrivenShaft::DrivenShaft(float periodSeconds)
        : periodSeconds(periodSeconds)
    {}

    void DrivenShaft::Drive(const std::optional<ShaftDrive>& drive)
    {
        this->drive = drive;
        impulseDeviation = drive ? Backlog(*drive) : 0.0f;
    }

    std::optional<foc::RadiansPerSecond> DrivenShaft::Speed() const
    {
        if (!drive)
            return std::nullopt;

        return foc::RadiansPerSecond{ drive->speed.Value() + impulseDeviation / drive->inertia.Value() };
    }

    void DrivenShaft::Record(foc::NewtonMeter electricalTorque)
    {
        torques[period % historyPeriods] = electricalTorque.Value();
        if (drive && period > drive->period)
            impulseDeviation += (electricalTorque.Value() - drive->torque.Value()) * periodSeconds;
        ++period;
    }

    float DrivenShaft::Backlog(const ShaftDrive& sent) const
    {
        if (sent.period >= period)
            return 0.0f;

        const auto oldestKept = period > historyPeriods ? period - historyPeriods : uint64_t{ 0 };
        float sum{ 0.0f };
        for (auto k = std::max(sent.period + 1, oldestKept); k < period; ++k)
            sum += torques[k % historyPeriods] - sent.torque.Value();
        return sum * periodSeconds;
    }
}
