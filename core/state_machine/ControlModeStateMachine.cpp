#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/OperatorTunings.hpp"
#include <variant>

namespace state_machine
{
    ControlModeStateMachine::ControlModeStateMachine(
        services::Tracer& tracer,
        const application::MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const application::CalibrationServices& calibServices,
        FaultNotifier& faultNotifier,
        const services::ConfigData& configData,
        OuterLoopArgs outerLoopArgs)
        : tracer(tracer)
        , hardware(hardware)
        , nvm(nvm)
        , calibServices(calibServices)
        , faultNotifier(faultNotifier)
        , outerLoopArgs(outerLoopArgs)
        , configData(configData)
        , configWriter(nvm, this->configData, tracer, nvmActivity)
        , algorithmPersistence(configWriter, this->configData, tracer)
        , drive(this->configData)
        , commands(drive, this->configData)
        , modes(drive, nvm, this->configData, nvmActivity, configWriter, [this](ControlMode mode)
              {
                  Activate(mode);
              })
        , transaction(drive, configWriter, modes)
        , tunings(drive, this->configData, configWriter, transaction, tracer)
        , algorithms(drive, algorithmPersistence, transaction, modes)
        , recalibration(drive, this->configData, nvm, nvmActivity, transaction, tunings, tracer)
    {
        SanitizeTunings(this->configData);
        SanitizeSupervision(this->configData);
        SanitizeLimits(this->configData);
        Activate(ControlModeFromRaw(configData.defaultControlMode));
    }

    ActiveDrive& ControlModeStateMachine::Drive()
    {
        return drive;
    }

    const ActiveDrive& ControlModeStateMachine::Drive() const
    {
        return drive;
    }

    DriveCommands& ControlModeStateMachine::Commands()
    {
        return commands;
    }

    ModeSelection& ControlModeStateMachine::Modes()
    {
        return modes;
    }

    SettingTransaction& ControlModeStateMachine::Settings()
    {
        return transaction;
    }

    TuningSettings& ControlModeStateMachine::Tunings()
    {
        return tunings;
    }

    AlgorithmSettings& ControlModeStateMachine::Algorithms()
    {
        return algorithms;
    }

    ModelRecalibration& ControlModeStateMachine::Recalibration()
    {
        return recalibration;
    }

    void ControlModeStateMachine::Activate(ControlMode mode)
    {
        const auto rotorReference = drive.RotorReferenceToKeep();
        auto activationArgs = outerLoopArgs;
        activationArgs.rotorReference = rotorReference;
        if (drive.HasMachine())
            drive.ActiveStateMachine().CmdEmergencyStop();
        recalibration.OnActivated();

        if (mode == ControlMode::speed)
            AttachAlgorithmRestore(drive.Emplace<application::SpeedStateMachine>(tracer, hardware, nvm, calibServices, faultNotifier, activationArgs, nvmActivity));
        else if (mode == ControlMode::position)
            AttachAlgorithmRestore(drive.Emplace<application::PositionStateMachine>(tracer, hardware, nvm, calibServices, faultNotifier, activationArgs, nvmActivity));
        else
            AttachAlgorithmRestore(drive.Emplace<application::TorqueStateMachine>(tracer, hardware, nvm, calibServices, faultNotifier, nvmActivity, rotorReference));

        commands.ApplyLimits();
    }

    void ControlModeStateMachine::OnFaultEntered(const infra::Function<void(FaultCode)>& onFaultEntered)
    {
        faultEntered = onFaultEntered;
    }

    void ControlModeStateMachine::AttachAlgorithmRestore(application::FocStateMachineCommon& stateMachine)
    {
        stateMachine.RegisterFaultEnteredHandler([this](FaultCode code)
            {
                if (faultEntered != nullptr)
                    faultEntered(code);
            });

        stateMachine.RegisterReadyHandler([this]()
            {
                algorithms.ApplyPersistedAlgorithms();
                tunings.ApplyOperatorTunings();
                recalibration.RecalibrateOnReady();
            });

        if (std::holds_alternative<Ready>(stateMachine.CurrentState()))
        {
            algorithms.ApplyPersistedAlgorithms();
            tunings.ApplyOperatorTunings();
            recalibration.RecalibrateOnReady();
        }
    }

    services::ConfigData& ControlModeStateMachine::Config()
    {
        return configData;
    }

    const services::ConfigData& ControlModeStateMachine::Config() const
    {
        return configData;
    }

    void ControlModeStateMachine::PersistConfig(const infra::Function<void(services::NvmStatus)>& onDone)
    {
        commands.ApplyLimits();
        configWriter.Persist(onDone);
    }

    bool ControlModeStateMachine::HasPendingConfigWrite() const
    {
        return modes.IsPending() || configWriter.HasPendingPersist();
    }

    void ControlModeStateMachine::HoldAsyncWork()
    {
        nvmActivity.Begin();
    }

    void ControlModeStateMachine::ReleaseAsyncWork()
    {
        nvmActivity.End();
    }

    foc::Weber ControlModeStateMachine::DefaultFluxLinkage() const
    {
        return calibServices.fluxLinkage;
    }
}
