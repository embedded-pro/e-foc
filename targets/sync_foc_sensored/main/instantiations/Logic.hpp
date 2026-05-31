#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocMotorCanBridge.hpp"
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
        static constexpr uint32_t controlLoopFrequencyHz = 10000;
        static constexpr uint32_t pwmDeadTimeNs = 500;

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
        state_machine::NoOpFaultNotifier noOpFaultNotifier;
        services::ConfigData configData;

        std::optional<services::CanFrameTransport> canTransport;
        std::optional<services::FocMotorCategoryServer> motorCanServer;
        std::optional<ControlMode> controlMode;
        std::optional<state_machine::FocMotorCanBridge> canBridge;
    };
}
