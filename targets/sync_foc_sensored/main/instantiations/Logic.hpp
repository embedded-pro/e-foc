#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "can-lite/tracing/TracingCan.hpp"
#include "can-lite/tracing/TracingCanProtocolServerObserver.hpp"
#include "core/can/CanLivenessWatchdog.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/cli/TerminalDiagnostics.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsetCompensation.hpp"
#include "core/services/dead_time_compensation/DeadTimeCompensation.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "core/supervision/EncoderPlausibilityMonitor.hpp"
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
        static constexpr uint32_t configRegionSize = 128;
        static_assert(services::NonVolatileMemoryImpl::calibrationRecordSize <= calibrationRegionSize);
        static_assert(services::NonVolatileMemoryImpl::configRecordSize <= configRegionSize);
        static constexpr uint32_t controlLoopFrequencyHz = 20000;
        static constexpr uint32_t pwmDeadTimeNs = 500;
        static constexpr float motorFluxLinkageWb = 0.007f;

        using ControlMode = state_machine::ControlModeStateMachine;

        application::PlatformFactory& hardware;
        uint8_t boardId;
        services::DebugLed debugLed;
        foc::Volts vdc;
        // help + 3 diagnostics + 20 control-mode commands; registering a 25th asserts at boot
        services::TerminalWithBanner::WithMaxSize<24> terminalWithStorage;
        services::TerminalDiagnostics terminalDiagnostics;
        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NonVolatileMemoryImpl nvm;
        infra::WithSharedAccess<services::PhaseCurrentOffsetCompensation> currentOffsets;
        services::DeadTimeCompensation deadTimeCompensation;
        infra::WithSharedAccess<services::ElectricalParametersIdentificationImpl> electricalIdent;
        infra::WithSharedAccess<services::MotorAlignmentImpl> motorAlignment;
        services::RotorStandstillImpl rotorStandstill;
        infra::WithSharedAccess<state_machine::PlatformFaultNotifier> platformFaultNotifier;
        supervision::EncoderPlausibilityMonitor encoderPlausibility;
        services::ConfigData configData;

        std::optional<services::TracingCan> tracingCan;
        std::optional<services::CanProtocolServer> canServer;
        std::optional<services::TracingCanProtocolServerObserver> tracingServerObserver;
        std::optional<can::FocMotorCategoryServer> motorCanServer;
        std::optional<ControlMode> controlMode;
        std::optional<can::FocMotorCanBridge> canBridge;
        std::optional<can::CanLivenessWatchdog> canLivenessWatchdog;
    };
}
