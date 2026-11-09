#pragma once

#include "core/foc/cascade/CurrentStream.hpp"
#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/bus_voltage/BusVoltageMonitor.hpp"
#include "core/services/fault_history/FaultHistory.hpp"
#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include "core/services/leds/StatusLeds.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include "targets/sync_foc_sensored/main/instantiations/CalibrationParts.hpp"
#include "targets/sync_foc_sensored/main/instantiations/CanServices.hpp"
#include "targets/sync_foc_sensored/main/instantiations/CanStack.hpp"
#include "targets/sync_foc_sensored/main/instantiations/ControlModeLedStatus.hpp"
#include "targets/sync_foc_sensored/main/instantiations/DriveSupervision.hpp"
#include "targets/sync_foc_sensored/main/instantiations/NvmStorage.hpp"
#include <optional>

namespace application
{
    class Logic
    {
    public:
        explicit Logic(application::PlatformFactory& hardware);

    private:
        static constexpr uint32_t controlLoopFrequencyHz = 20000;
        static constexpr uint32_t pwmDeadTimeNs = 500;
        static constexpr float motorFluxLinkageWb = 0.007f;

        using ControlMode = state_machine::ControlModeStateMachine;

        void OnConfigLoaded(services::NvmStatus status);
        void StartCan();
        void StartControl();
        void StartCanServices();
        void BroadcastFaults();
        void RecordFault(state_machine::FaultCode code);

        application::PlatformFactory& hardware;
        uint8_t boardId;
        ControlModeLedStatus ledStatus;
        services::StatusLeds statusLeds;
        foc::Volts vdc;
        NvmStorage storage;
        application::DecimatingThreePhaseInverter inverter;
        services::InverterCompensation inverterCompensation;
        CalibrationParts calibration;
        infra::WithSharedAccess<state_machine::PlatformFaultNotifier> platformFaultNotifier;
        DriveSupervision supervision;
        foc::Volts filteredBusVoltage{ 0.0f };
        services::BusVoltageMonitor busVoltage;
        services::ConfigData configData;
        foc::CurrentStream currentStream;
        std::optional<CanStack> canStack;
        std::optional<ControlMode> controlMode;
        std::optional<services::FaultHistory> faultHistory;
        std::optional<CanServices> canServices;
    };
}
