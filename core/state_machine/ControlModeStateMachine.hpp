#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/AlgorithmPersistence.hpp"
#include "core/state_machine/AlgorithmSettings.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/DriveCommands.hpp"
#include "core/state_machine/ModeSelection.hpp"
#include "core/state_machine/ModelRecalibration.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include "core/state_machine/SettingTransaction.hpp"
#include "core/state_machine/TuningSettings.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/util/Function.hpp"

namespace state_machine
{
    class ControlModeStateMachine
    {
    public:
        using OuterLoopArgs = application::OuterLoopArgs;
        using OnSetting = SettingTransaction::OnSetting;

        ControlModeStateMachine(
            services::Tracer& tracer,
            const application::MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const application::CalibrationServices& calibServices,
            FaultNotifier& faultNotifier,
            const services::ConfigData& configData,
            OuterLoopArgs outerLoopArgs);

        ActiveDrive& Drive();
        const ActiveDrive& Drive() const;
        DriveCommands& Commands();
        ModeSelection& Modes();
        SettingTransaction& Settings();
        TuningSettings& Tunings();
        AlgorithmSettings& Algorithms();
        ModelRecalibration& Recalibration();

        void OnFaultEntered(const infra::Function<void(FaultCode)>& onFaultEntered);
        services::ConfigData& Config();
        const services::ConfigData& Config() const;
        void PersistConfig(const infra::Function<void(services::NvmStatus)>& onDone);
        bool HasPendingConfigWrite() const;
        void HoldAsyncWork();
        void ReleaseAsyncWork();
        foc::Weber DefaultFluxLinkage() const;

    private:
        void Activate(ControlMode mode);
        void AttachAlgorithmRestore(application::FocStateMachineCommon& stateMachine);

        services::Tracer& tracer;
        const application::MotorHardware hardware;
        services::NonVolatileMemory& nvm;
        const application::CalibrationServices calibServices;
        FaultNotifier& faultNotifier;
        OuterLoopArgs outerLoopArgs;

        services::ConfigData configData;
        application::NvmActivity nvmActivity;
        ConfigWriter configWriter;
        AlgorithmPersistence algorithmPersistence;
        ActiveDrive drive;
        DriveCommands commands;
        ModeSelection modes;
        SettingTransaction transaction;
        TuningSettings tunings;
        AlgorithmSettings algorithms;
        ModelRecalibration recalibration;
        infra::Function<void(FaultCode)> faultEntered;
    };
}
