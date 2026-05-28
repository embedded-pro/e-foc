#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/cli/TerminalWithBanner.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "services/peripheral/DebugLed.hpp"
#include <optional>

namespace application
{
#ifdef E_FOC_AUTO_TRANSITION_POLICY
    static constexpr state_machine::TransitionPolicy selectedTransitionPolicy = state_machine::TransitionPolicy::Auto;
#else
    static constexpr state_machine::TransitionPolicy selectedTransitionPolicy = state_machine::TransitionPolicy::Cli;
#endif

    template<typename FocImpl, typename TerminalImpl>
    class LogicWithOuterLoop
    {
    public:
        explicit LogicWithOuterLoop(application::PlatformFactory& hardware, infra::BoundedConstString bannerName)
            : hardware{ hardware }
            , debugLed{ hardware.Leds().front(), std::chrono::milliseconds(50), std::chrono::milliseconds(1950) }
            , vdc{ hardware.PowerSupplyVoltage() }
            , terminalWithStorage{ hardware.Terminal(), hardware.Tracer(), services::TerminalWithBanner::Banner{ bannerName, vdc, hardware.SystemClock(), hardware.GetResetCause(), hardware.FaultStatus() } }
            , calibrationRegion{ hardware.Eeprom(), calibrationRegionOffset, calibrationRegionSize }
            , configRegion{ hardware.Eeprom(), configRegionOffset, configRegionSize }
            , nvm{ calibrationRegion, configRegion }
            , electricalIdent{ hardware, hardware, vdc }
            , motorAlignment{ hardware, hardware }
        {
            hardware.ConfigureAdcAndPwm(hal::Hertz{ controlLoopFrequencyHz }, std::chrono::nanoseconds{ pwmDeadTimeNs }, PlatformFactory::SampleAndHold::shorter);
            nvm.LoadConfig(configData, [this](services::NvmStatus status)
                {
                    if (status != services::NvmStatus::Ok)
                        configData = services::MakeDefaultConfigData();
                    this->hardware.SetEncoderResolution(this->configData.encoderResolution);
                    this->hardware.ConfigureCanBus(this->configData.canBaudrate, false);
                    motorStateMachine.emplace(
                        TerminalAndTracer{ terminalWithStorage, this->hardware.Tracer() },
                        MotorHardware{ this->hardware, this->hardware, vdc },
                        nvm,
                        CalibrationServices{ electricalIdent, motorAlignment },
                        noOpFaultNotifier,
                        selectedTransitionPolicy,
                        this->hardware.MaxCurrentSupported(), this->hardware.BaseFrequency(), this->hardware.LowPriorityInterrupt());
                    terminalInteractor.emplace(terminalWithStorage, vdc, motorStateMachine->GetFoc());
                });
        }

    private:
        static constexpr uint32_t calibrationRegionOffset = 0;
        static constexpr uint32_t calibrationRegionSize = 128;
        static constexpr uint32_t configRegionOffset = calibrationRegionOffset + calibrationRegionSize;
        static constexpr uint32_t configRegionSize = 128;
        static constexpr uint32_t controlLoopFrequencyHz = 10000;
        static constexpr uint32_t pwmDeadTimeNs = 500;

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
        std::optional<FocStateMachineImpl<FocImpl>> motorStateMachine;
        std::optional<TerminalImpl> terminalInteractor;
    };
}
