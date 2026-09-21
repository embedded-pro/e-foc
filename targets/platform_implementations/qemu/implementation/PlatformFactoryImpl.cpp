#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr int kResponseLinesPerDrain = 16;

    long Milli(float value)
    {
        return std::lround(value * 1000.0f);
    }

    long Micro(float value)
    {
        return std::lround(value * 1000000.0f);
    }
}

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

        const uint32_t baseHz = BaseFrequencyFrom(plantConfig).Value();

        if (plantConfig->responseSampleRateHz != 0)
        {
            really_assert(baseHz % plantConfig->responseSampleRateHz == 0);
            responseRecorder.Configure(baseHz / plantConfig->responseSampleRateHz, plantConfig->responseMaxSamples);
        }

        torqueStep.Configure({ plantConfig->torqueStepNm,
            static_cast<uint32_t>(static_cast<uint64_t>(plantConfig->torqueStepDelayMs) * baseHz / 1000u) });
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
        responseDrainTimer.Start(std::chrono::milliseconds(1), [this]()
            {
                DrainResponseRecords();
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

    void PlatformFactoryImpl::ConfigureAdcAndPwm(hal::Hertz, std::chrono::nanoseconds, SampleAndHold)
    {
        const auto freq = baseFrequency;

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
                    // Read before dispatch: the stamp is the response window's onset, so it must not fall after the setpoint took effect.
                    const uint32_t tick = controlTick.load(std::memory_order_relaxed);
                    if (const auto frame = canBusAdapter->PollIncoming())
                        StampReceivedFrame(tick, *frame);
                });
        }
    }

    CanBusAdapter& PlatformFactoryImpl::CanBus()
    {
        return *canBusAdapter;
    }

    OPTIMIZE_FOR_SPEED void PlatformFactoryImpl::PhaseCurrentsReady(hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
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
        const uint32_t tick = controlTick.load(std::memory_order_relaxed) + 1;
        controlTick.store(tick, std::memory_order_relaxed);

        if (const auto torque = torqueStep.Fire(tick))
        {
            model.SetExternalTorque(foc::NewtonMeter{ *torque });
            responseRecorder.Note(foc::PlantResponseKind::torqueStep, PlantSampleAt(tick));
        }

        model.StepForTest(lastDutyPhases);
        responseRecorder.Capture(PlantSampleAt(tick));

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
        responseRecorder.Begin();
        torqueStep.Begin();
        boardProtection.SetArmed(true);
    }

    void PlatformFactoryImpl::Stop()
    {
        boardProtection.SetArmed(false);
        model.Stop();
        model.SetExternalTorque(foc::NewtonMeter{ 0.0f });
        responseRecorder.End();
    }

    foc::PlantResponseSample PlatformFactoryImpl::PlantSampleAt(uint32_t tick) const
    {
        const auto dq = model.LastDqCurrents();
        return foc::PlantResponseSample{ tick, model.MechanicalSpeed().Value(), model.MechanicalAngle().Value(), dq.q, dq.d, model.ExternalTorque().Value() };
    }

    void PlatformFactoryImpl::DrainResponseRecords()
    {
        bool printed = false;

        for (int i = 0; i != kResponseLinesPerDrain; ++i)
        {
            const auto record = responseRecorder.Pop();
            if (!record)
                break;

            if (!printed)
                std::putchar('\n');
            printed = true;

            PrintResponseRecord(*record);
        }

        if (printed)
            std::fflush(stdout);
    }

    void PlatformFactoryImpl::PrintResponseRecord(const foc::PlantResponseRecord& record)
    {
        const auto& sample = record.sample;
        const auto tick = static_cast<unsigned long>(sample.tick);

        switch (record.kind)
        {
            case foc::PlantResponseKind::sample:
                std::printf("PLANT %lu %ld %ld %ld %ld %ld\n", tick, Milli(sample.omegaMech), Micro(sample.thetaMech), Micro(sample.iq), Micro(sample.id), Micro(sample.externalTorqueNm));
                break;
            case foc::PlantResponseKind::began:
                std::printf("PLANT_START %lu\n", tick);
                break;
            case foc::PlantResponseKind::torqueStep:
                std::printf("PLANT_EVENT %lu torque %ld\n", tick, Micro(sample.externalTorqueNm));
                break;
            case foc::PlantResponseKind::stopped:
                std::printf("PLANT_STOP %lu %lu\n", tick, static_cast<unsigned long>(record.dropped));
                break;
        }
    }

    void PlatformFactoryImpl::StampReceivedFrame(uint32_t tick, const sil::SemihostingCan::Frame& frame)
    {
        char hexData[17]{};
        int pos = 0;
        for (const uint8_t byte : frame.message)
        {
            hexData[pos++] = "0123456789abcdef"[(byte >> 4) & 0xF];
            hexData[pos++] = "0123456789abcdef"[byte & 0xF];
        }
        hexData[pos] = '\0';

        const uint32_t rawId = frame.id.Is11BitId() ? frame.id.Get11BitId() : frame.id.Get29BitId();
        std::printf("\nCAN_RX_AT %lu %lx %s\n", static_cast<unsigned long>(tick), static_cast<unsigned long>(rawId), hexData);
        std::fflush(stdout);
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
