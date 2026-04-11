#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FaultNotifier.hpp"
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
    {};

    struct Calibrating
    {
        CalibrationStep step{ CalibrationStep::polePairs };
        services::CalibrationData pendingData{};
    };

    struct Ready
    {
        services::CalibrationData loadedData{};
    };

    struct Enabled
    {};

    struct Fault
    {
        FaultCode code{ FaultCode::hardwareFault };
    };

    using State = std::variant<Idle, Calibrating, Ready, Enabled, Fault>;

    class FocStateMachineBase
    {
    public:
        virtual const State& CurrentState() const = 0;
        virtual FaultCode LastFaultCode() const = 0;
        virtual void CmdCalibrate() = 0;
        virtual void CmdEnable() = 0;
        virtual void CmdDisable() = 0;
        virtual void CmdClearFault() = 0;
        virtual void CmdClearCalibration() = 0;

        virtual void ApplyOnlineEstimates()
        {}
    };
}
