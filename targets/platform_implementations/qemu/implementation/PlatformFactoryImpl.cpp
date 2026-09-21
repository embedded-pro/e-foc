#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <chrono>
#include <cstdio>

namespace application
{
    foc::ThreePhaseMotorModel::Parameters PlatformFactoryImpl::MotorParametersFrom(
        const std::optional<sil::SilPlantConfig>& config,
        const foc::ThreePhaseMotorModel::Parameters& fallback)
    {
        if (!config.has_value())
            return fallback;

        return foc::ThreePhaseMotorModel::Parameters{
            .R = foc::Ohm{ config->statorResistanceOhm },
            .Ld = foc::Henry{ config->dAxisInductanceHenry },
            .Lq = foc::Henry{ config->qAxisInductanceHenry },
            .psi_f = foc::Weber{ config->fluxLinkageWeber },
            .p = config->polePairs,
            .J = foc::KilogramMeterSquared{ config->rotorInertiaKgM2 },
            .B = foc::NewtonMeterSecondPerRadian{ config->viscousDampingNmSPerRad },
            .maxSupportedCurrent = foc::Ampere{ config->maxSupportedCurrentAmpere },
        };
    }

    foc::Volts PlatformFactoryImpl::SupplyVoltageFrom(const std::optional<sil::SilPlantConfig>& config)
    {
        return config.has_value() ? foc::Volts{ config->powerSupplyVoltageVolts } : foc::Volts{ kDefaultSupplyVoltageVolts };
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequencyFrom(const std::optional<sil::SilPlantConfig>& config)
    {
        if (!config.has_value() || config->baseFrequencyHz == 0)
            return hal::Hertz{ kDefaultBaseFrequencyHz };

        return hal::Hertz{ config->baseFrequencyHz };
    }

    void PlatformFactoryImpl::ApplyPlantConfig()
    {
        if (!plantConfig.has_value())
            return;

        model.SetRandomSeed(plantConfig->randomSeed);
        model.SetAdcNoise({
            .sigmaAmpere = plantConfig->noiseSigmaAmpere,
            .biasAmpereA = plantConfig->noiseBiasAmpereA,
            .biasAmpereB = plantConfig->noiseBiasAmpereB,
            .biasAmpereC = plantConfig->noiseBiasAmpereC,
        });
        model.SetThermalConfig({
            .ambientCelsius = plantConfig->ambientCelsius,
            .thermalResistance = plantConfig->thermalResistance,
            .thermalCapacitance = plantConfig->thermalCapacitance,
            .copperTempCoeff = plantConfig->copperTempCoeff,
            .ironInductanceCoeff = plantConfig->ironInductanceCoeff,
        });
        model.SetEncoderNoise({
            .sigmaRadians = plantConfig->encoderSigmaRadians,
            .biasRadians = plantConfig->encoderBiasRadians,
        });
        model.SetFaultInjection({
            .openPhaseA = (plantConfig->faultFlags & sil::PlantFaultFlag::openPhaseA) != 0,
            .openPhaseB = (plantConfig->faultFlags & sil::PlantFaultFlag::openPhaseB) != 0,
            .openPhaseC = (plantConfig->faultFlags & sil::PlantFaultFlag::openPhaseC) != 0,
            .encoderStuck = (plantConfig->faultFlags & sil::PlantFaultFlag::encoderStuck) != 0,
            .supplyVoltageScale = plantConfig->supplyVoltageScale,
        });
        model.ResetTemperature();

        boardProtection.Configure({
            .overCurrentAmpere = plantConfig->overCurrentTripAmpere,
            .overVoltageVolts = plantConfig->overVoltageTripVolts,
            .underVoltageVolts = plantConfig->underVoltageTripVolts,
            .overTemperatureCelsius = plantConfig->overTemperatureTripCelsius,
        });

        if (plantConfig->loadTorqueNm != 0.0f)
            model.SetLoad(foc::NewtonMeter{ plantConfig->loadTorqueNm });
    }

    PlatformFactoryImpl::PlatformFactoryImpl(const foc::ThreePhaseMotorModel::Parameters& motorParams,
        const infra::Function<void()>& onInit)
        : plantConfig(LoadSilPlantConfig(sil::plantConfigFileName))
        , onInitialized(onInit)
        , focTimer(0x40000000u, 8, kQemuSystemClockHz, BaseFrequencyFrom(plantConfig).Value(), [this]()
              {
                  FocTimerIsr();
              })
        , model(MotorParametersFrom(plantConfig, motorParams), SupplyVoltageFrom(plantConfig), BaseFrequencyFrom(plantConfig), std::nullopt, false)
        , baseFrequency(BaseFrequencyFrom(plantConfig))
    {
        services::SetGlobalTracerInstance(terminalAndTracer.tracer);
        ApplyPlantConfig();
        onInitialized();
        protectionPollTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                boardProtection.DeliverPendingProtection();
            });
        focTimer.Start();
    }

    void PlatformFactoryImpl::Run()
    {
        std::puts("\nREADY");
        std::fflush(stdout);

        while (true)
            cortex.eventDispatcher.ExecuteAllActions();
    }

    services::Tracer& PlatformFactoryImpl::Tracer()
    {
        return terminalAndTracer.tracer;
    }

    services::TerminalWithCommands& PlatformFactoryImpl::Terminal()
    {
        return terminalAndTracer.terminal;
    }

    hal::GpioPin& PlatformFactoryImpl::OperationalLed()
    {
        return operationalPin;
    }

    hal::GpioPin& PlatformFactoryImpl::WarningLed()
    {
        return warningPin;
    }

    hal::GpioPin& PlatformFactoryImpl::FailureLed()
    {
        return failurePin;
    }

    uint8_t PlatformFactoryImpl::BoardId() const
    {
        return 0;
    }

    bool PlatformFactoryImpl::PowerStatus() const
    {
        return true;
    }

    hal::PerformanceTracker& PlatformFactoryImpl::PerformanceTimer()
    {
        return performanceTracker;
    }

    hal::Hertz PlatformFactoryImpl::SystemClock() const
    {
        return hal::Hertz{ kQemuSystemClockHz };
    }

    foc::Volts PlatformFactoryImpl::PowerSupplyVoltage()
    {
        return model.EffectiveSupplyVoltage();
    }

    foc::LowPriorityInterrupt& PlatformFactoryImpl::LowPriorityInterrupt()
    {
        return pendSvLowPriorityInterrupt;
    }

    hal::Eeprom& PlatformFactoryImpl::Eeprom()
    {
        return eeprom;
    }

    drivers::Watchdog& PlatformFactoryImpl::Watchdog()
    {
        return watchdog;
    }

    void PlatformFactoryImpl::RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection)
    {
        boardProtection.Register(onProtection);
    }

    PlatformFactory::BoardProtectionState PlatformFactoryImpl::BoardProtectionStatus()
    {
        return boardProtection.Status();
    }

    void PlatformFactoryImpl::Reset()
    {}

    void PlatformFactoryImpl::ResetFromWatchdogExpiry()
    {
        Reset();
    }

    ResetCause PlatformFactoryImpl::GetResetCause() const
    {
        return ResetCause::powerUp;
    }

    infra::BoundedConstString PlatformFactoryImpl::FaultStatus() const
    {
        return {};
    }

    PlatformDiagnostics& PlatformFactoryImpl::Diagnostics()
    {
        return diagnostics;
    }

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz freq, std::chrono::nanoseconds, SampleAndHold)
    {
        baseFrequency = freq;

        const auto periodCycles = freq.Value() == 0
                                      ? 0u
                                      : static_cast<uint32_t>(kQemuSystemClockHz / freq.Value());
        controlLoopMetrics.Configure(static_cast<uint32_t>(static_cast<uint64_t>(periodCycles) * 3u / 4u), periodCycles);
    }

    void PlatformFactoryImpl::SetEncoderResolution(uint32_t)
    {}

    void PlatformFactoryImpl::ConfigureCanBus(uint32_t, bool)
    {
        if (!canBusAdapter)
        {
            canBusAdapter.emplace();
            diagnostics.AttachCanBus(*canBusAdapter);
            canPollTimer.Start(std::chrono::milliseconds(1), [this]()
                {
                    canBusAdapter->PollIncoming();
                });
        }
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBusAdapter;
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::PhaseCurrentsReady(hal::Hertz freq, const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        baseFrequency = freq;
        onPhaseCurrentsReadyValid = false;
        onPhaseCurrentsReady = onDone;
        onPhaseCurrentsReadyValid = true;
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastDutyPhases = dutyPhases;
    }

    void PlatformFactoryImpl::FocTimerIsr()
    {
        model.StepForTest(lastDutyPhases);

        lastCurrents = model.LastMeasuredCurrents();

        if (onPhaseCurrentsReadyValid && onPhaseCurrentsReady && !controlLoopEntered)
        {
            controlLoopEntered = true;
            const auto entryCycles = CycleCounter::Now();

            onPhaseCurrentsReady(lastCurrents);

            controlLoopMetrics.Record(CycleCounter::Now() - entryCycles);
            controlLoopEntered = false;
        }
        else if (onPhaseCurrentsReadyValid && onPhaseCurrentsReady && controlLoopEntered)
            controlLoopMetrics.RecordReentry();

        boardProtection.Evaluate(lastCurrents, model.EffectiveSupplyVoltage().Value(), model.WindingTemperatureCelsius());
    }

    void PlatformFactoryImpl::Start()
    {
        model.Start();
        boardProtection.SetArmed(true);
    }

    void PlatformFactoryImpl::Stop()
    {
        boardProtection.SetArmed(false);
        model.Stop();
    }

    hal::Hertz PlatformFactoryImpl::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Ampere PlatformFactoryImpl::MaxCurrentSupported() const
    {
        return model.MaxCurrentSupported();
    }

    OPTIMIZE_FOR_SPEED foc::Radians PlatformFactoryImpl::Read()
    {
        return model.Read();
    }

    void PlatformFactoryImpl::Set(foc::Radians value)
    {
        model.Set(value);
    }

    void PlatformFactoryImpl::SetZero()
    {
        model.SetZero();
    }
}
