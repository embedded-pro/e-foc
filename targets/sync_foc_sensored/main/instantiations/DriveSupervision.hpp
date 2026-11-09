#pragma once

#include "core/can/CanCommandTimeout.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "core/supervision/BusVoltageLimitMonitor.hpp"
#include "core/supervision/EncoderFreezeMonitor.hpp"
#include "core/supervision/EncoderPlausibilityMonitor.hpp"
#include "core/supervision/FollowingErrorMonitor.hpp"
#include "core/supervision/MotorOverload.hpp"
#include "core/supervision/OverTemperature.hpp"
#include "core/supervision/OverspeedMonitor.hpp"
#include "core/supervision/PeakCurrent.hpp"
#include "core/supervision/PhaseLossMonitor.hpp"
#include "core/supervision/PositionLimitMonitor.hpp"
#include "core/supervision/TargetReachedMonitor.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <optional>

namespace application
{
    class DriveSupervision
    {
    public:
        using ControlMode = state_machine::ControlModeStateMachine;

        DriveSupervision(PlatformFactory& hardware, std::optional<ControlMode>& controlMode, infra::WithSharedAccess<state_machine::PlatformFaultNotifier>& faultNotifier);

        void AttachPeakCurrent();
        void SuperviseDrive();
        void SuperviseEncoder();
        void SuperviseMotion();

        void CommandAccepted();
        void EvaluateBusVoltage(foc::Volts measured);
        bool IsTargetReached() const;
        foc::Ampere PeakCurrent() const;

    private:
        void Raise(state_machine::FaultCode code);
        void ApplyCurrentLimit();
        ControlMode& Control() const;

        PlatformFactory& hardware;
        std::optional<ControlMode>& controlMode;
        infra::WithSharedAccess<state_machine::PlatformFaultNotifier>& faultNotifier;
        supervision::EncoderPlausibilityMonitor encoderPlausibility;
        supervision::EncoderFreezeMonitor encoderFreeze;
        supervision::PhaseLossMonitor phaseLoss;
        supervision::OverspeedMonitor overspeed;
        supervision::FollowingErrorMonitor followingError;
        supervision::TargetReachedMonitor targetReached;
        supervision::PositionLimitMonitor positionLimit;
        can::CanCommandTimeout commandTimeout;
        supervision::MotorOverload motorOverload;
        supervision::OverTemperature overTemperature;
        foc::Ampere overloadLimit;
        foc::Ampere thermalLimit;
        supervision::BusVoltageLimitMonitor busVoltageLimits;
        supervision::PeakCurrent peakCurrent;
    };
}
