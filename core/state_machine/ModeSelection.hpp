#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace state_machine
{
    class ModeSelection
    {
    public:
        ModeSelection(const ActiveDrive& drive, services::NonVolatileMemory& nvm, services::ConfigData& config, application::NvmActivity& nvmActivity, const ConfigWriter& configWriter, const infra::Function<void(ControlMode)>& activate);

        void Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone);
        bool IsPending() const;
        bool CanSwitch() const;

    private:
        void OnSaveConfigDone(services::NvmStatus status);
        void OnRollbackSaveConfigDone();

        const ActiveDrive& drive;
        services::NonVolatileMemory& nvm;
        services::ConfigData& config;
        application::NvmActivity& nvmActivity;
        const ConfigWriter& configWriter;
        infra::Function<void(ControlMode)> activate;
        ControlMode pendingMode{ ControlMode::torque };
        infra::AutoResetFunction<void(SelectResult)> pendingCallback;
        uint8_t previousDefaultControlMode{ 0 };
    };
}
