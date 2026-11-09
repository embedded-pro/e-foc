#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <cstdint>
#include <map>
#include <optional>

namespace tool
{
    // A fault-history entry arrives as a record frame followed by a conditions frame of the same age.
    class FaultRecordAssembler
    {
    public:
        void Record(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs);
        std::optional<can::FocFaultRecord> Conditions(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent);
        void Forget(uint16_t nodeId);

    private:
        std::map<uint16_t, can::FocFaultRecord> records;
    };
}
