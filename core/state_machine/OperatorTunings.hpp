#pragma once

#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"

namespace state_machine
{
    foc::CurrentLoopTunings StoredCurrentTunings(const services::ConfigData& config);
    foc::SpeedLoopTunings StoredSpeedTunings(const services::ConfigData& config);
    foc::PositionLoopTunings StoredPositionTunings(const services::ConfigData& config);

    void StoreTunings(services::ConfigData& config, const foc::CurrentLoopTunings& tunings);
    void StoreTunings(services::ConfigData& config, const foc::SpeedLoopTunings& tunings);
    void StoreTunings(services::ConfigData& config, const foc::PositionLoopTunings& tunings);

    bool IsAcceptable(const foc::CurrentLoopTunings& tunings);
    bool IsAcceptable(const foc::SpeedLoopTunings& tunings);
    bool IsAcceptable(const foc::PositionLoopTunings& tunings);

    bool IsDesignable(const foc::CurrentLoopTunings& tunings);

    void SanitizeTunings(services::ConfigData& config);
    void SanitizeSupervision(services::ConfigData& config);
}
