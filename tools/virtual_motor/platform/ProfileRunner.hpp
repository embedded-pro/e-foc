#pragma once

#include "tools/virtual_motor/platform/InstrumentReports.hpp"
#include "tools/virtual_motor/platform/PlantDisturbances.hpp"
#include "tools/virtual_motor/platform/TriggerWatch.hpp"
#include <optional>
#include <vector>

namespace virtual_motor
{
    class ProfileRunner
    {
    public:
        explicit ProfileRunner(PlantDisturbances& disturbances);

        void Report(InstrumentReports& reports);

        std::optional<port::RefusalReason> Run(const port::ProfileRequest& profile);
        void Stop();
        void Abandon();
        bool Active() const;

        void Observe(uint64_t period, const port::Sample& sample, uint8_t leds, bool protection);

    private:
        std::optional<port::RefusalReason> Check(const port::ProfileRequest& profile) const;
        bool ApplyDue();
        void State(port::ProfileStateKind state);

        PlantDisturbances& disturbances;
        InstrumentReports* reports{ nullptr };
        TriggerWatch watch;
        std::vector<port::ProfileStep> steps;
        std::size_t next{ 0 };
        uint64_t start{ 0 };
        uint32_t elapsed{ 0 };
        bool armed{ false };
        bool running{ false };
    };
}
