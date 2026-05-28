#pragma once

#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
#include "core/foc/instantiations/FocController.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/cli/TerminalBase.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <bit>
#include <cmath>
#include <optional>
#include <type_traits>

#ifdef E_FOC_STATE_MACHINE_COVERAGE_BUILD
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#endif

namespace application
{
    struct TerminalAndTracer
    {
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
    };

    struct MotorHardware
    {
        foc::ThreePhaseInverter& inverter;
        foc::Encoder& encoder;
        foc::Volts vdc;
    };

    struct CalibrationServices
    {
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;
        services::MechanicalParametersIdentification* mechIdentOverride{ nullptr };
        foc::NewtonMeter mechTorqueConstant{ foc::NewtonMeter{ 0.1f } };
    };

    // True when FocImpl has an outer velocity loop (FocSpeed or FocPosition).
    template<typename FocImpl>
    inline constexpr bool HasSpeedLoop = std::is_base_of_v<foc::FocSpeed, FocImpl> || std::is_base_of_v<foc::FocPosition, FocImpl>;

    // True only for FocSpeed: MechanicalParametersIdentificationImpl requires foc::FocSpeed&.
    // FocPosition must supply a mechIdentOverride in CalibrationServices.
    template<typename FocImpl>
    inline constexpr bool HasAutoMechIdent = std::is_base_of_v<foc::FocSpeed, FocImpl>;

    template<typename FocImpl>
    class FocStateMachineImpl
        : public state_machine::FocStateMachineBase
    {
        static_assert(std::is_base_of_v<foc::FocBase, FocImpl>, "FocImpl must inherit from foc::FocBase");

        static constexpr bool hasMechanicalIdent = HasSpeedLoop<FocImpl>;
        static constexpr float nyquistFactor = 15.0f;
        static constexpr float velocityBandwidthRadPerSec = 50.0f;

    public:
        template<typename... FocArgs>
        FocStateMachineImpl(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy,
            FocArgs&&... focArgs);

        const state_machine::State& CurrentState() const override;
        state_machine::FaultCode LastFaultCode() const override;

        void CmdCalibrate() override;
        void CmdEnable() override;
        void CmdDisable() override;
        void CmdClearFault() override;
        void CmdClearCalibration() override;
        void ApplyOnlineEstimates() override;
        FocImpl& GetFoc();

    private:
        void EnterCalibrating();
        void EnterReady(const services::CalibrationData& data);
        void EnterEnabled();
        void EnterFault(state_machine::FaultCode code);

        void RunPolePairsStep();
        void RunResistanceAndInductanceStep();
        void RunAlignmentStep();
        void RunMechanicalIdentStep();
        void OnCalibrationComplete();

        bool IsCalibrating(state_machine::CalibrationStep expected) const;

        void ApplyCalibrationData(const services::CalibrationData& data);
        void CheckNvmOnBoot();
        void OnNvmValidationResult(bool valid);
        void OnNvmLoadResult(services::NvmStatus status);

        void RegisterCliCommands();
        void ApplyOnlineEstimatesImpl();

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        foc::ThreePhaseInverter& inverter;
        foc::Encoder& encoder;
        foc::Volts vdc;
        services::NonVolatileMemory& nvm;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;

        foc::FocController<FocImpl> focController;
        foc::WithAutomaticCurrentPidGains pidAutoTuner{ focController };

        state_machine::State currentState{ state_machine::Idle{} };
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::hardwareFault };
        services::CalibrationData calibrationData{};
        foc::NewtonMeter mechTorqueConstant;
        services::MechanicalParametersIdentification* mechIdent{ nullptr };

        std::optional<services::MechanicalParametersIdentificationImpl> optMechIdent;
        std::optional<services::RealTimeFrictionAndInertiaEstimator> optOnlineMechEstimator;
        std::optional<services::RealTimeResistanceAndInductanceEstimator> optOnlineElecEstimator;
        std::optional<foc::WithAutomaticSpeedPidGains> optSpeedAutoTuner;
    };

    // Implementation

    template<typename FocImpl>
    template<typename... FocArgs>
    FocStateMachineImpl<FocImpl>::FocStateMachineImpl(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        state_machine::FaultNotifier& faultNotifier,
        state_machine::TransitionPolicy transitionPolicy,
        FocArgs&&... focArgs)
        : terminal(terminalAndTracer.terminal)
        , tracer(terminalAndTracer.tracer)
        , inverter(hardware.inverter)
        , encoder(hardware.encoder)
        , vdc(hardware.vdc)
        , nvm(nvm)
        , electricalIdent(calibServices.electricalIdent)
        , motorAlignment(calibServices.motorAlignment)
        , focController(inverter, encoder, std::forward<FocArgs>(focArgs)...)
        , mechTorqueConstant(calibServices.mechTorqueConstant)
    {
        if constexpr (hasMechanicalIdent)
        {
            if (calibServices.mechIdentOverride != nullptr)
                mechIdent = calibServices.mechIdentOverride;
            else if constexpr (HasAutoMechIdent<FocImpl>)
            {
                optMechIdent.emplace(static_cast<foc::FocSpeed&>(focController), inverter, encoder);
                mechIdent = &*optMechIdent;
            }

            optOnlineMechEstimator.emplace(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, inverter.BaseFrequency());
            optOnlineElecEstimator.emplace(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, inverter.BaseFrequency());
            optSpeedAutoTuner.emplace(static_cast<foc::FocSpeedTunable&>(focController));

            static_cast<foc::FocOnlineEstimableBase&>(focController).SetOnlineMechanicalEstimator(*optOnlineMechEstimator);
            static_cast<foc::FocOnlineEstimableBase&>(focController).SetOnlineElectricalEstimator(*optOnlineElecEstimator);
        }

        faultNotifier.Register([this](state_machine::FaultCode code)
            {
                EnterFault(code);
            });

        if (transitionPolicy == state_machine::TransitionPolicy::Cli)
        {
            RegisterCliCommands();
        }

        CheckNvmOnBoot();
    }

    template<typename FocImpl>
    const state_machine::State& FocStateMachineImpl<FocImpl>::CurrentState() const
    {
        return currentState;
    }

    template<typename FocImpl>
    state_machine::FaultCode FocStateMachineImpl<FocImpl>::LastFaultCode() const
    {
        return lastFaultCode;
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CmdCalibrate()
    {
        if (!std::holds_alternative<state_machine::Idle>(currentState) &&
            !std::holds_alternative<state_machine::Ready>(currentState))
        {
            return;
        }
        EnterCalibrating();
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CmdEnable()
    {
        if (!std::holds_alternative<state_machine::Ready>(currentState))
        {
            return;
        }
        EnterEnabled();
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CmdDisable()
    {
        if (!std::holds_alternative<state_machine::Enabled>(currentState))
        {
            return;
        }
        focController.Stop();
        EnterReady(calibrationData);
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CmdClearFault()
    {
        if (!std::holds_alternative<state_machine::Fault>(currentState))
        {
            return;
        }
        tracer.Trace() << "[SM] Fault cleared";
        currentState = state_machine::Idle{};
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CmdClearCalibration()
    {
        if (!std::holds_alternative<state_machine::Idle>(currentState) &&
            !std::holds_alternative<state_machine::Ready>(currentState))
        {
            return;
        }

        nvm.InvalidateCalibration([this](services::NvmStatus status)
            {
                if (!std::holds_alternative<state_machine::Idle>(currentState) &&
                    !std::holds_alternative<state_machine::Ready>(currentState))
                    return;

                if (status != services::NvmStatus::Ok)
                {
                    EnterFault(state_machine::FaultCode::hardwareFault);
                    return;
                }
                tracer.Trace() << "[SM] Calibration cleared";
                currentState = state_machine::Idle{};
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::EnterCalibrating()
    {
        tracer.Trace() << "[SM] Entering Calibrating";
        currentState = state_machine::Calibrating{};
        RunPolePairsStep();
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::EnterReady(const services::CalibrationData& data)
    {
        tracer.Trace() << "[SM] Entering Ready";
        calibrationData = data;
        currentState = state_machine::Ready{ data };
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::EnterEnabled()
    {
        tracer.Trace() << "[SM] Entering Enabled";
        if constexpr (hasMechanicalIdent)
        {
            if (optOnlineMechEstimator.has_value())
                optOnlineMechEstimator->SetTorqueConstant(mechTorqueConstant);
        }
        focController.Start();
        currentState = state_machine::Enabled{};
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::EnterFault(state_machine::FaultCode code)
    {
        tracer.Trace() << "[SM] Entering Fault";
        if (std::holds_alternative<state_machine::Enabled>(currentState) ||
            std::holds_alternative<state_machine::Calibrating>(currentState))
        {
            focController.Stop();
        }
        lastFaultCode = code;
        currentState = state_machine::Fault{ code };
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::RunPolePairsStep()
    {
        tracer.Trace() << "[SM] Identifying pole pairs";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::polePairs;

        electricalIdent.EstimateNumberOfPolePairs({}, [this](std::optional<std::size_t> result)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::polePairs))
                    return;
                if (!result.has_value())
                {
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto& cal = std::get<state_machine::Calibrating>(currentState);
                cal.pendingData.polePairs = static_cast<uint8_t>(*result);
                RunResistanceAndInductanceStep();
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::RunResistanceAndInductanceStep()
    {
        tracer.Trace() << "[SM] Identifying resistance and inductance";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::resistanceAndInductance;

        electricalIdent.EstimateResistanceAndInductance({},
            [this](std::optional<foc::Ohm> r, std::optional<foc::MilliHenry> l)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::resistanceAndInductance))
                    return;
                if (!r || !l)
                {
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto& cal = std::get<state_machine::Calibrating>(currentState);
                cal.pendingData.rPhase = r->Value();
                cal.pendingData.lD = l->Value();
                cal.pendingData.lQ = l->Value();
                RunAlignmentStep();
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::RunAlignmentStep()
    {
        tracer.Trace() << "[SM] Aligning motor";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::alignment;
        const auto polePairs = calibrating.pendingData.polePairs;

        motorAlignment.ForceAlignment(polePairs, {},
            [this](std::optional<foc::Radians> angle)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::alignment))
                    return;
                if (!angle)
                {
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto& cal = std::get<state_machine::Calibrating>(currentState);
                cal.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle->Value());
                if constexpr (hasMechanicalIdent)
                    RunMechanicalIdentStep();
                else
                    OnCalibrationComplete();
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::OnCalibrationComplete()
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return;

        auto pendingData = std::get<state_machine::Calibrating>(currentState).pendingData;

        nvm.SaveCalibration(pendingData,
            [this](services::NvmStatus status)
            {
                if (!std::holds_alternative<state_machine::Calibrating>(currentState))
                    return;
                if (status != services::NvmStatus::Ok)
                {
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto data = std::get<state_machine::Calibrating>(currentState).pendingData;
                ApplyCalibrationData(data);
                EnterReady(data);
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::RunMechanicalIdentStep()
    {
        tracer.Trace() << "[SM] Estimating mechanical parameters";

        if (mechIdent == nullptr)
        {
            EnterFault(state_machine::FaultCode::calibrationFailed);
            return;
        }

        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::frictionAndInertia;
        const auto polePairs = static_cast<std::size_t>(calibrating.pendingData.polePairs);

        mechIdent->EstimateFrictionAndInertia(
            mechTorqueConstant,
            polePairs,
            services::MechanicalParametersIdentification::Config{},
            [this](std::optional<foc::NewtonMeterSecondPerRadian> friction,
                std::optional<foc::NewtonMeterSecondSquared> inertia)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::frictionAndInertia))
                    return;
                if (!friction || !inertia)
                {
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto& cal = std::get<state_machine::Calibrating>(currentState);
                cal.pendingData.inertia = inertia->Value();
                cal.pendingData.frictionViscous = friction->Value();
                cal.pendingData.kpVelocity = inertia->Value() * velocityBandwidthRadPerSec;
                cal.pendingData.kiVelocity = friction->Value() * velocityBandwidthRadPerSec;
                OnCalibrationComplete();
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::ApplyCalibrationData(const services::CalibrationData& data)
    {
        focController.SetPolePairs(data.polePairs);
        encoder.Set(foc::Radians{ std::bit_cast<float>(data.encoderZeroOffset) });
        pidAutoTuner.SetPidBasedOnResistanceAndInductance(
            vdc, foc::Ohm{ data.rPhase }, foc::MilliHenry{ data.lD },
            inverter.BaseFrequency(), nyquistFactor);
        if constexpr (hasMechanicalIdent)
        {
            focController.SetSpeedTunings(vdc, foc::SpeedTunings{ data.kpVelocity, data.kiVelocity, 0.0f });

            if (optOnlineMechEstimator.has_value() && data.inertia > 0.0f)
            {
                optOnlineMechEstimator->SetInitialEstimate(
                    foc::NewtonMeterSecondSquared{ data.inertia },
                    foc::NewtonMeterSecondPerRadian{ data.frictionViscous });
            }
            if (optOnlineElecEstimator.has_value())
            {
                // Seed using lD: the online electrical estimator assumes a non-salient motor (Ld ≈ Lq).
                optOnlineElecEstimator->SetInitialEstimate(
                    foc::Ohm{ data.rPhase },
                    foc::MilliHenry{ data.lD });
            }
        }
    }

    template<typename FocImpl>
    bool FocStateMachineImpl<FocImpl>::IsCalibrating(state_machine::CalibrationStep expected) const
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return false;
        return std::get<state_machine::Calibrating>(currentState).step == expected;
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::CheckNvmOnBoot()
    {
        nvm.IsCalibrationValid([this](bool valid)
            {
                OnNvmValidationResult(valid);
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::OnNvmValidationResult(bool valid)
    {
        if (!std::holds_alternative<state_machine::Idle>(currentState))
            return;
        if (!valid)
        {
            tracer.Trace() << "[SM] NVM invalid, starting in Idle";
            return;
        }

        nvm.LoadCalibration(calibrationData, [this](services::NvmStatus status)
            {
                OnNvmLoadResult(status);
            });
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::OnNvmLoadResult(services::NvmStatus status)
    {
        if (!std::holds_alternative<state_machine::Idle>(currentState))
            return;
        if (status != services::NvmStatus::Ok)
        {
            tracer.Trace() << "[SM] NVM load failed, starting in Idle";
            return;
        }
        ApplyCalibrationData(calibrationData);
        EnterReady(calibrationData);
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::ApplyOnlineEstimates()
    {
        if (!std::holds_alternative<state_machine::Enabled>(currentState))
            return;
        ApplyOnlineEstimatesImpl();
    }

    template<typename FocImpl>
    FocImpl& FocStateMachineImpl<FocImpl>::GetFoc()
    {
        return focController;
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::ApplyOnlineEstimatesImpl()
    {
        if constexpr (hasMechanicalIdent)
        {
            if (optSpeedAutoTuner.has_value() && optOnlineMechEstimator.has_value())
            {
                const auto inertia = optOnlineMechEstimator->CurrentInertia();
                const auto friction = optOnlineMechEstimator->CurrentFriction();
                if (!std::isfinite(inertia.Value()) || inertia.Value() <= 0.0f ||
                    !std::isfinite(friction.Value()) || friction.Value() <= 0.0f)
                {
                    tracer.Trace() << "[SM] Skipping mechanical estimates: non-physical values (J=" << inertia.Value() << " B=" << friction.Value() << ")";
                }
                else
                {
                    tracer.Trace() << "[SM] Applying mechanical estimates: J=" << inertia.Value() << " B=" << friction.Value();
                    optSpeedAutoTuner->SetPidBasedOnInertiaAndFriction(vdc, inertia, friction, velocityBandwidthRadPerSec);
                }
            }
            if (optOnlineElecEstimator.has_value())
            {
                const auto resistance = optOnlineElecEstimator->CurrentResistance();
                const auto inductance = optOnlineElecEstimator->CurrentInductance();
                if (!std::isfinite(resistance.Value()) || resistance.Value() <= 0.0f ||
                    !std::isfinite(inductance.Value()) || inductance.Value() <= 0.0f)
                {
                    tracer.Trace() << "[SM] Skipping electrical estimates: non-physical values (R=" << resistance.Value() << " L=" << inductance.Value() << ")";
                }
                else
                {
                    tracer.Trace() << "[SM] Applying electrical estimates: R=" << resistance.Value() << " L=" << inductance.Value();
                    pidAutoTuner.SetPidBasedOnResistanceAndInductance(
                        vdc, resistance, inductance, inverter.BaseFrequency(), nyquistFactor);
                }
            }
        }
    }

    template<typename FocImpl>
    void FocStateMachineImpl<FocImpl>::RegisterCliCommands()
    {
        terminal.AddCommand({ { "calibrate", "cal", "Run full calibration sequence" },
            [this](const infra::BoundedConstString&)
            {
                CmdCalibrate();
            } });

        terminal.AddCommand({ { "enable", "en", "Enable FOC controller" },
            [this](const infra::BoundedConstString&)
            {
                CmdEnable();
            } });

        terminal.AddCommand({ { "disable", "dis", "Disable FOC controller" },
            [this](const infra::BoundedConstString&)
            {
                CmdDisable();
            } });

        terminal.AddCommand({ { "clear_fault", "cf", "Clear fault and return to Idle" },
            [this](const infra::BoundedConstString&)
            {
                CmdClearFault();
            } });

        terminal.AddCommand({ { "clear_cal", "cc", "Clear calibration data from NVM" },
            [this](const infra::BoundedConstString&)
            {
                CmdClearCalibration();
            } });

        if constexpr (hasMechanicalIdent)
        {
            terminal.AddCommand({ { "apply_estimates", "ae", "Apply online estimates to PID gains" },
                [this](const infra::BoundedConstString&)
                {
                    ApplyOnlineEstimates();
                } });

            terminal.AddCommand({ { "estimate_status", "es", "Print current online estimates" },
                [this](const infra::BoundedConstString&)
                {
                    if (optOnlineMechEstimator.has_value())
                        tracer.Trace() << "[EST] Mech: J=" << optOnlineMechEstimator->CurrentInertia().Value()
                                       << " B=" << optOnlineMechEstimator->CurrentFriction().Value();
                    if (optOnlineElecEstimator.has_value())
                        tracer.Trace() << "[EST] Elec: R=" << optOnlineElecEstimator->CurrentResistance().Value()
                                       << " L=" << optOnlineElecEstimator->CurrentInductance().Value();
                } });
        }
    }
}

#ifdef E_FOC_STATE_MACHINE_COVERAGE_BUILD

namespace application
{
    extern template class FocStateMachineImpl<foc::FocTorqueImpl>;
    extern template class FocStateMachineImpl<foc::FocSpeedImpl>;
    extern template class FocStateMachineImpl<foc::FocPositionImpl>;
}
#endif
