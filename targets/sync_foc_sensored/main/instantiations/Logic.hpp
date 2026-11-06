#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeCategoryServer.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/tracing/TracingCan.hpp"
#include "can-lite/tracing/TracingCanProtocolServerObserver.hpp"
#include "core/can/CanCommandTimeout.hpp"
#include "core/can/CanLivenessWatchdog.hpp"
#include "core/can/FirmwareUpgradeInterlock.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorCurrentStreamer.hpp"
#include "core/can/UntracedStreamCan.hpp"
#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/bus_voltage/BusVoltageMonitor.hpp"
#include "core/services/cli/StartupBanner.hpp"
#include "core/services/current_offsets/ConfirmedPhaseCurrentOffsets.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsetMeasurement.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/fault_history/FaultHistory.hpp"
#include "core/services/firmware_upgrade/FirmwareUpgrade.hpp"
#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "core/supervision/BusVoltageLimitMonitor.hpp"
#include "core/supervision/EncoderFreezeMonitor.hpp"
#include "core/supervision/EncoderPlausibilityMonitor.hpp"
#include "core/supervision/FollowingErrorMonitor.hpp"
#include "core/supervision/MotorOverload.hpp"
#include "core/supervision/OverTemperature.hpp"
#include "core/supervision/OverspeedMonitor.hpp"
#include "core/supervision/PeakCurrent.hpp"
#include "core/supervision/PhaseLossMonitor.hpp"
#include "core/supervision/PositionLimitMonitor.hpp"
#include "core/supervision/TargetReachedMonitor.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include "services/peripheral/DebugLed.hpp"
#include <optional>

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        static constexpr uint32_t calibrationRegionOffset = 0;
        static constexpr uint32_t calibrationRegionSize = 128;
        static constexpr uint32_t configRegionOffset = calibrationRegionOffset + calibrationRegionSize;
        static constexpr uint32_t configRegionSize = 224;
        static constexpr uint32_t learnedModelRegionOffset = configRegionOffset + configRegionSize;
        static constexpr uint32_t learnedModelRegionSize = 32;
        static constexpr uint32_t faultHistoryRegionOffset = learnedModelRegionOffset + learnedModelRegionSize;
        static constexpr uint32_t faultHistoryRegionSize = 128;
        static_assert(services::NonVolatileMemoryImpl::calibrationRecordSize <= calibrationRegionSize);
        static_assert(services::NonVolatileMemoryImpl::configRecordSize <= configRegionSize);
        static_assert(services::learnedModelImageSize <= learnedModelRegionSize);
        static_assert(services::faultHistoryImageSize <= faultHistoryRegionSize);
        static_assert(faultHistoryRegionOffset == 384, "the fault history must not move");
        static constexpr uint32_t controlLoopFrequencyHz = 20000;

        void ApplyCurrentLimit();
        void RecordFault(state_machine::FaultCode code);
        static constexpr uint32_t pwmDeadTimeNs = 500;
        static constexpr float motorFluxLinkageWb = 0.007f;

        using ControlMode = state_machine::ControlModeStateMachine;

        application::PlatformFactory& hardware;
        uint8_t boardId;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NvmEepromRegion learnedModelRegion;
        services::NvmEepromRegion faultHistoryRegion;
        services::NonVolatileMemoryImpl nvm;
        application::DecimatingThreePhaseInverter inverter;
        services::InverterCompensation inverterCompensation;
        infra::WithSharedAccess<services::PhaseCurrentOffsetMeasurement> currentOffsets;
        services::ConfirmedPhaseCurrentOffsets confirmedCurrentOffsets;
        infra::WithSharedAccess<services::ElectricalParametersIdentificationImpl> electricalIdent;
        infra::WithSharedAccess<services::MotorAlignmentImpl> motorAlignment;
        services::RotorStandstillImpl rotorStandstill;
        infra::WithSharedAccess<state_machine::PlatformFaultNotifier> platformFaultNotifier;
        supervision::EncoderPlausibilityMonitor encoderPlausibility;
        supervision::EncoderFreezeMonitor encoderFreeze;
        supervision::PhaseLossMonitor phaseLoss;
        supervision::OverspeedMonitor overspeed;
        supervision::FollowingErrorMonitor followingError;
        supervision::TargetReachedMonitor targetReached;
        supervision::PositionLimitMonitor positionLimit;
        can::CanCommandTimeout commandTimeout;
        supervision::MotorOverload motorOverload;
        supervision::OverTemperature overTemperature;
        foc::Ampere overloadLimit;
        foc::Ampere thermalLimit;
        supervision::BusVoltageLimitMonitor busVoltageLimits;
        supervision::PeakCurrent peakCurrent;
        foc::Volts filteredBusVoltage{ 0.0f };
        services::BusVoltageMonitor busVoltage;
        services::ConfigData configData;
        foc::CurrentStream currentStream;

        std::optional<services::TracingCan> tracingCan;
        std::optional<can::UntracedStreamCan> canBus;
        std::optional<services::CanProtocolServer> canServer;
        std::optional<services::TracingCanProtocolServerObserver> tracingServerObserver;
        std::optional<can::FocMotorCategoryServer> motorCanServer;
        std::optional<services::FirmwareUpgradeCategoryServer> firmwareUpgradeServer;
        std::optional<ControlMode> controlMode;
        std::optional<services::FaultHistory> faultHistory;
        std::optional<can::FirmwareUpgradeInterlock> firmwareUpgradeInterlock;
        std::optional<services::FirmwareUpgrade> firmwareUpgrade;
        std::optional<can::FocMotorCanBridge> canBridge;
        std::optional<can::CanLivenessWatchdog> canLivenessWatchdog;
        std::optional<can::FocMotorCurrentStreamer> currentStreamer;
    };
}
