#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "core/state_machine/ModeSelection.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace state_machine
{
    enum class TuningResult : uint8_t
    {
        ok,
        outOfRange,
        wrongMode,
        notStopped
    };

    enum class SettingResult : uint8_t
    {
        ok,
        busy,
        invalidState,
        wrongMode,
        outOfRange,
        invalidParameters,
        nvmFailed,
        aborted
    };

    class SettingTransaction
    {
    public:
        using OnSetting = infra::Function<void(SettingResult)>;

        SettingTransaction(const ActiveDrive& drive, ConfigWriter& configWriter, const ModeSelection& modes);

        SettingResult Gate() const;
        bool IsIdle() const;
        void Begin(const OnSetting& onDone);
        void StoreAndPersist(const OnSetting& onDone);
        void Complete(SettingResult result);
        void CompleteWith(services::NvmStatus status);

        static SettingResult ToSettingResult(foc::SelectResult result);
        static SettingResult ToSettingResult(CommandResult result);

    private:
        const ActiveDrive& drive;
        ConfigWriter& configWriter;
        const ModeSelection& modes;
        OnSetting pending;
    };
}
