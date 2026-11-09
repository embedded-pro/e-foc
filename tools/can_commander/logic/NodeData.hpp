#pragma once

#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/logic/FaultRecordAssembler.hpp"
#include "tools/can_commander/logic/FirmwareIdentityCollector.hpp"
#include "tools/can_commander/logic/ReadScheduler.hpp"
#include <cstdint>
#include <optional>

namespace tool
{
    class NodeData
    {
    public:
        explicit NodeData(can::FocMotorCategoryClient& category);

        bool RequestData(uint16_t nodeId);
        void RequestSettings(uint16_t nodeId);
        void RequestReadings(uint16_t nodeId);
        void RequestStatus(uint16_t nodeId);
        void RequestFirmwareIdentity(uint16_t nodeId);
        void RequestFaultHistory(uint16_t nodeId);

        void RequestAfterContract(uint16_t nodeId);
        void RequestAfterAccepted(uint16_t nodeId, uint8_t commandType);

        void ReadAnswered(uint16_t nodeId, uint8_t commandType);
        std::optional<FirmwareIdentity> ReadRefused(uint16_t nodeId, uint8_t commandType);
        std::optional<FirmwareIdentity> ParameterReceived(uint16_t nodeId, can::FocParameter parameter, float value);
        void FaultRecordReceived(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs);
        std::optional<can::FocFaultRecord> FaultConditionsReceived(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent);
        void Forget(uint16_t nodeId);

    private:
        bool IsCompatible(uint16_t nodeId) const;
        bool IsIdentityQueued(uint16_t nodeId) const;
        bool Send(uint16_t nodeId, const NodeRead& read);

        can::FocMotorCategoryClient& category;
        ReadScheduler reads;
        FirmwareIdentityCollector identities;
        FaultRecordAssembler faultRecords;
    };
}
