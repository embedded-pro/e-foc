#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeSession.hpp"
#include "core/can/CanLivenessWatchdog.hpp"
#include "core/can/FirmwareUpgradeInterlock.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCurrentStreamer.hpp"
#include "core/foc/cascade/CurrentStream.hpp"
#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/fault_history/FaultHistory.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "targets/sync_foc_sensored/main/instantiations/CalibrationParts.hpp"
#include "targets/sync_foc_sensored/main/instantiations/CanStack.hpp"

namespace application
{
    struct CanServices
    {
        struct Wiring
        {
            CanStack& canStack;
            state_machine::ControlModeStateMachine& controlMode;
            PlatformFactory& hardware;
            DecimatingThreePhaseInverter& inverter;
            CalibrationParts& calibration;
            services::FaultHistory& faultHistory;
            foc::CurrentStream& currentStream;
            hal::Hertz baseFrequency;
        };

        explicit CanServices(const Wiring& wiring);

        can::FirmwareUpgradeInterlock firmwareUpgradeInterlock;
        services::FirmwareUpgradeSession firmwareUpgrade;
        can::FocMotorCanBridge canBridge;
        can::CanLivenessWatchdog canLivenessWatchdog;
        can::FocMotorCurrentStreamer currentStreamer;
    };
}
