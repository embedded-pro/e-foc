#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/util/Function.hpp"
#include <optional>
#include <variant>

namespace state_machine
{
    class CommandCallback
    {
    public:
        using Function = infra::Function<void(CommandResult)>;

        CommandCallback() = default;
        CommandCallback(const Function& function);
        CommandCallback(const CommandCallback& other) noexcept;
        CommandCallback(CommandCallback&& other) noexcept;
        CommandCallback& operator=(const CommandCallback& other) noexcept;
        CommandCallback& operator=(CommandCallback&& other) noexcept;
        ~CommandCallback() = default;

        const Function& Callback() const;

    private:
        Function function;
    };

    inline CommandCallback::CommandCallback(const Function& function)
        : function(function)
    {}

    inline CommandCallback::CommandCallback(const CommandCallback& other) noexcept
        : function(other.function)
    {}

    inline CommandCallback::CommandCallback(CommandCallback&& other) noexcept
        : function(other.function)
    {}

    inline CommandCallback& CommandCallback::operator=(const CommandCallback& other) noexcept
    {
        function = other.function;
        return *this;
    }

    inline CommandCallback& CommandCallback::operator=(CommandCallback&& other) noexcept
    {
        function = other.function;
        return *this;
    }

    inline const CommandCallback::Function& CommandCallback::Callback() const
    {
        return function;
    }

    struct Calibrate
    {
        static constexpr const char* name{ "Calibrate" };
        CommandCallback onDone;
    };

    struct ReAlign
    {
        static constexpr const char* name{ "ReAlign" };
        CommandCallback onDone;
    };

    struct ReserveExternalCalibration
    {
        static constexpr const char* name{ "ReserveExternalCalibration" };
    };

    struct CompleteExternalCalibration
    {
        static constexpr const char* name{ "CompleteExternalCalibration" };
        services::CalibrationData data;
        CommandCallback onDone;
    };

    struct RunCalibrationSequence
    {
        static constexpr const char* name{ "RunCalibrationSequence" };
    };

    struct RunAlignmentOnly
    {
        static constexpr const char* name{ "RunAlignmentOnly" };
    };

    struct CalibrationStepChanged
    {
        static constexpr const char* name{ "CalibrationStepChanged" };
        CalibrationStep step;
    };

    struct AlignmentSucceeded
    {
        static constexpr const char* name{ "AlignmentSucceeded" };
        foc::Radians angle;
    };

    struct CalibrationStepFailed
    {
        static constexpr const char* name{ "CalibrationStepFailed" };
    };

    struct MechanicalParametersIdentified
    {
        static constexpr const char* name{ "MechanicalParametersIdentified" };
        std::optional<foc::NewtonMeterSecondPerRadian> friction;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
        float speedLoopBandwidth;
    };

    struct CalibrationSaved
    {
        static constexpr const char* name{ "CalibrationSaved" };
        services::NvmStatus status;
    };

    struct Enable
    {
        static constexpr const char* name{ "Enable" };
    };

    struct Disable
    {
        static constexpr const char* name{ "Disable" };
    };

    struct ClearFault
    {
        static constexpr const char* name{ "ClearFault" };
    };

    struct EmergencyStop
    {
        static constexpr const char* name{ "EmergencyStop" };
    };

    struct FaultDetected
    {
        static constexpr const char* name{ "FaultDetected" };
        FaultCode code;
    };

    struct ClearCalibration
    {
        static constexpr const char* name{ "ClearCalibration" };
        CommandCallback onDone;
    };

    struct CalibrationInvalidated
    {
        static constexpr const char* name{ "CalibrationInvalidated" };
        services::NvmStatus status;
    };

    struct SetFluxLinkage
    {
        static constexpr const char* name{ "SetFluxLinkage" };
        foc::Weber fluxLinkage;
        CommandCallback onDone;
    };

    struct FluxLinkageSaved
    {
        static constexpr const char* name{ "FluxLinkageSaved" };
        services::NvmStatus status;
    };

    struct BootValidityChecked
    {
        static constexpr const char* name{ "BootValidityChecked" };
        bool valid;
    };

    struct BootCalibrationLoaded
    {
        static constexpr const char* name{ "BootCalibrationLoaded" };
        services::NvmStatus status;
    };

    using Event = std::variant<
        Calibrate,
        ReAlign,
        ReserveExternalCalibration,
        CompleteExternalCalibration,
        RunCalibrationSequence,
        RunAlignmentOnly,
        CalibrationStepChanged,
        AlignmentSucceeded,
        CalibrationStepFailed,
        MechanicalParametersIdentified,
        CalibrationSaved,
        Enable,
        Disable,
        ClearFault,
        EmergencyStop,
        FaultDetected,
        ClearCalibration,
        CalibrationInvalidated,
        SetFluxLinkage,
        FluxLinkageSaved,
        BootValidityChecked,
        BootCalibrationLoaded>;
}
