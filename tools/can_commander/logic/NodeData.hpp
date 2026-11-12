#pragma once

#include "e-foc-protocol/client/FocMotorCategoryClient.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "tools/can_commander/logic/FaultRecordAssembler.hpp"
#include "tools/can_commander/logic/FirmwareIdentityCollector.hpp"
#include "tools/can_commander/logic/ReadScheduler.hpp"
#include <cstdint>
#include <map>
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

        void SetMinor(uint16_t nodeId, uint8_t minor);
        void RequestAfterContract(uint16_t nodeId);
        void RequestAfterAccepted(uint16_t nodeId, uint8_t commandType);

        void ReadAnswered(uint16_t nodeId, uint8_t commandType);
        std::optional<FirmwareIdentity> ReadRefused(uint16_t nodeId, uint8_t commandType);
        std::optional<FirmwareIdentity> ParameterReceived(uint16_t nodeId, can::FocParameter parameter, float value);
        void FaultRecordReceived(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs);
        std::optional<can::FocFaultRecord> FaultConditionsReceived(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent);
        void Forget(uint16_t nodeId);

    private:
        static constexpr uint8_t supervisionContractMinor{ 4 };
        static constexpr uint8_t limitsContractMinor{ 9 };
        static constexpr uint8_t targetWindowContractMinor{ 10 };
        static constexpr uint8_t travelLimitsContractMinor{ 11 };
        static constexpr uint8_t thermalContractMinor{ 13 };
        static constexpr uint8_t overTemperatureContractMinor{ 14 };
        static constexpr uint8_t busVoltageLimitsContractMinor{ 15 };
        static constexpr uint8_t quickStopContractMinor{ 16 };

        bool IsCompatible(uint16_t nodeId) const;
        bool SupportsFeature(uint16_t nodeId, uint8_t threshold) const;
        bool SupportsSetting(uint16_t nodeId, can::FocParameter parameter) const;
        bool IsIdentityQueued(uint16_t nodeId) const;
        bool Send(uint16_t nodeId, const NodeRead& read);

        can::FocMotorCategoryClient& category;
        ReadScheduler reads;
        FirmwareIdentityCollector identities;
        FaultRecordAssembler faultRecords;
        std::map<uint16_t, uint8_t> nodeMinors;
    };
}
