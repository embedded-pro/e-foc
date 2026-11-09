#include "tools/can_commander/logic/FaultRecordAssembler.hpp"

namespace tool
{
    void FaultRecordAssembler::Record(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs)
    {
        records[nodeId] = can::FocFaultRecord{ .age = age, .code = code, .mode = mode, .uptimeMs = uptimeMs };
    }

    std::optional<can::FocFaultRecord> FaultRecordAssembler::Conditions(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent)
    {
        const auto partial = records.find(nodeId);
        if (partial == records.end() || partial->second.age != age)
            return std::nullopt;

        auto record = partial->second;
        records.erase(partial);
        record.powerUp = powerUp;
        record.busVoltage = busVoltage;
        record.peakCurrent = peakCurrent;
        return record;
    }

    void FaultRecordAssembler::Forget(uint16_t nodeId)
    {
        records.erase(nodeId);
    }
}
