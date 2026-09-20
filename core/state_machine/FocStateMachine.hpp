#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>
#include <variant>

namespace state_machine
{
    enum class CalibrationStep : uint8_t
    {
        polePairs,
        resistanceAndInductance,
        alignment,
        frictionAndInertia
    };

    struct Idle
    {
        static constexpr const char* name{ "Idle" };
    };

    struct Calibrating
    {
        static constexpr const char* name{ "Calibrating" };

        CalibrationStep step{ CalibrationStep::polePairs };
        services::CalibrationData pendingData{};
        bool alignmentOnly{ false };
    };

    struct Ready
    {
        static constexpr const char* name{ "Ready" };

        services::CalibrationData loadedData{};
        bool rotorReferenceValid{ false };
    };

    struct Enabled
    {
        static constexpr const char* name{ "Enabled" };
    };

    struct Fault
    {
        static constexpr const char* name{ "Fault" };

        FaultCode code{ FaultCode::hardwareFault };
    };

    using State = std::variant<Idle, Calibrating, Ready, Enabled, Fault>;

    enum class CommandResult : uint8_t
    {
        ok,
        rejected,
        calibrationFailed,
        nvmFailed,
        abortedByFault
    };

    inline bool IsStopped(const State& state)
    {
        return std::holds_alternative<Idle>(state) || std::holds_alternative<Ready>(state);
    }

    class FocStateMachineBase
    {
    public:
        virtual ~FocStateMachineBase() = default;
        virtual const State& CurrentState() const = 0;
        virtual FaultCode LastFaultCode() const = 0;

        virtual bool HasPendingAsyncWork() const = 0;
        virtual bool HasPartialCalibration() const = 0;

        virtual void CmdCalibrate(const infra::Function<void(CommandResult)>& onDone) = 0;
        virtual CommandResult CmdEnable() = 0;
        virtual CommandResult CmdDisable() = 0;
        virtual CommandResult CmdClearFault() = 0;
        virtual void CmdClearCalibration(const infra::Function<void(CommandResult)>& onDone) = 0;
        virtual CommandResult CmdEmergencyStop() = 0;

        virtual void ApplyOnlineEstimates()
        {}
    };
}
