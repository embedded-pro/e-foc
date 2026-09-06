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
#include "core/services/cli/TerminalWithBanner.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
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
        static constexpr uint32_t controlLoopFrequencyHz = 20000;
        static constexpr uint32_t pwmDeadTimeNs = 500;
        static constexpr float motorFluxLinkageWb = 0.007f;
        static constexpr float motorTorqueConstantNm = 0.1f;

        using ControlMode = state_machine::ControlModeStateMachine;

        application::PlatformFactory& hardware;
        services::DebugLed debugLed;
        foc::Volts vdc;
        services::TerminalWithBanner::WithMaxSize<20> terminalWithStorage;
        services::NvmEepromRegion calibrationRegion;
        services::NvmEepromRegion configRegion;
        services::NonVolatileMemoryImpl nvm;
        services::ElectricalParametersIdentificationImpl electricalIdent;
        services::MotorAlignmentImpl motorAlignment;
        state_machine::PlatformFaultNotifier platformFaultNotifier;
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
