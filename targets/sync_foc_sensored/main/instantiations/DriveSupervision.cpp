#include "targets/sync_foc_sensored/main/instantiations/DriveSupervision.hpp"
#include <algorithm>
#include <chrono>

namespace application
{
    DriveSupervision::DriveSupervision(PlatformFactory& hardware, std::optional<ControlMode>& controlMode, infra::WithSharedAccess<state_machine::PlatformFaultNotifier>& faultNotifier)
        : hardware{ hardware }
        , controlMode{ controlMode }
        , faultNotifier{ faultNotifier }
        , encoderPlausibility{ hardware, hardware.MaxCurrentSupported(), [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , encoderFreeze{ hardware, [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , phaseLoss{ hardware.MaxCurrentSupported(), [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , overspeed{ [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , followingError{ [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , positionLimit{ [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , commandTimeout{ [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , motorOverload{ hardware.MaxCurrentSupported(), [this](foc::Ampere limit)
            {
                overloadLimit = limit;
                ApplyCurrentLimit();
            } }
        , overTemperature{ hardware.MaxCurrentSupported(), [this](foc::Ampere limit)
            {
                thermalLimit = limit;
                ApplyCurrentLimit();
            },
            [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
        , overloadLimit{ hardware.MaxCurrentSupported() }
        , thermalLimit{ hardware.MaxCurrentSupported() }
        , busVoltageLimits{ [this](state_machine::FaultCode code)
            {
                Raise(code);
            } }
    {}

    void DriveSupervision::AttachPeakCurrent()
    {
        peakCurrent.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
    }

    void DriveSupervision::SuperviseDrive()
    {
        commandTimeout.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        motorOverload.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        motorOverload.Enable([this]()
            {
                return supervision::OverloadRatingsFrom(Control().Config());
            });
        overTemperature.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            },
            [this]()
            {
                return Control().Drive().WindingTemperature();
            });
        overTemperature.Enable([this]()
            {
                return supervision::OverTemperatureSettingsFrom(Control().Config());
            });
        busVoltageLimits.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        busVoltageLimits.Enable([this]()
            {
                return supervision::BusVoltageLimitSettingsFrom(Control().Config());
            });
        commandTimeout.Enable([this]()
            {
                return std::chrono::duration_cast<infra::Duration>(std::chrono::duration<float>(Control().Config().limits.commandTimeout));
            });
    }

    void DriveSupervision::SuperviseEncoder()
    {
        encoderPlausibility.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        encoderPlausibility.Enable([this]()
            {
                return supervision::ConfigFrom(Control().Config());
            });
        encoderFreeze.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        encoderFreeze.Enable([this]()
            {
                return supervision::LimitsFrom(Control().Drive().CalibrationRecord(), Control().Config().encoderResolution, hardware.MaxCurrentSupported());
            });
        phaseLoss.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        phaseLoss.Enable([this]()
            {
                return foc::RadiansPerSecond{ Control().Config().currentLoop.bandwidth };
            });
    }

    void DriveSupervision::SuperviseMotion()
    {
        overspeed.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        overspeed.Enable([this]()
            {
                return foc::RadiansPerSecond{ Control().Config().limits.overspeedLimit };
            });
        followingError.Attach([this]() -> const state_machine::FocStateMachineBase&
            {
                return Control().Drive().ActiveStateMachine();
            });
        followingError.Enable([this]()
            {
                return supervision::FollowingErrorSettingsFrom(Control().Config());
            });
        targetReached.Attach([this]() -> const state_machine::FocStateMachineBase*
            {
                return Control().Drive().Active() == state_machine::ControlMode::position ? &Control().Drive().ActiveStateMachine() : nullptr;
            });
        targetReached.Enable([this]()
            {
                return supervision::TargetReachedSettingsFrom(Control().Config());
            });
        positionLimit.Attach([this]() -> const state_machine::FocStateMachineBase*
            {
                return Control().Drive().Active() == state_machine::ControlMode::position ? &Control().Drive().ActiveStateMachine() : nullptr;
            },
            [this]()
            {
                return static_cast<drivers::Encoder&>(hardware).Read();
            });
        positionLimit.Enable([this]()
            {
                return supervision::PositionLimitSettingsFrom(Control().Config());
            });
    }

    void DriveSupervision::CommandAccepted()
    {
        commandTimeout.CommandAccepted();
    }

    void DriveSupervision::EvaluateBusVoltage(foc::Volts measured)
    {
        busVoltageLimits.Evaluate(measured);
    }

    bool DriveSupervision::IsTargetReached() const
    {
        return targetReached.IsReached();
    }

    foc::Ampere DriveSupervision::PeakCurrent() const
    {
        return peakCurrent.Peak();
    }

    void DriveSupervision::Raise(state_machine::FaultCode code)
    {
        faultNotifier->Raise(code);
    }

    void DriveSupervision::ApplyCurrentLimit()
    {
        const auto limit = foc::Ampere{ std::min(overloadLimit.Value(), thermalLimit.Value()) };
        hardware.Tracer().Trace() << "[PRT] Current limit " << limit.Value() << " A";
        Control().Commands().LimitCurrent(limit);
    }

    DriveSupervision::ControlMode& DriveSupervision::Control() const
    {
        return *controlMode;
    }
}
