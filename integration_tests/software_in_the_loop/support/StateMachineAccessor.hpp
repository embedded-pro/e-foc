#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <functional>

namespace integration
{
    // StateMachineAccessor is populated by each fixture's GIVEN step and consumed
    // by generic step definitions (StateMachineSteps, CalibrationSteps, CanSteps).
    // Using std::function<> is acceptable on host-side SIL tests.
    struct StateMachineAccessor
    {
        state_machine::FocStateMachineBase* stateMachine{ nullptr };

        std::function<void()> executeAll;
        std::function<void()> setupCalibrationExpectations;
        std::function<void()> setupCanIntegration;
        std::function<void()> injectCanStart;
        std::function<void()> injectCanStop;
        std::function<void()> injectCanClearFault;
        std::function<void()> triggerHardwareFault;

        // Coordinator-specific (populated only by ControlModeCoordinationFixture).
        std::function<void()> setupCanIntegrationWithCoordinator;
        std::function<void(services::FocMotorMode)> injectCanSelectControlMode;
        std::function<void(int16_t)> injectCanSetTorqueSetpoint;
        std::function<void(int16_t)> injectCanSetSpeedSetpoint;
        std::function<void(int16_t)> injectCanSetPositionSetpoint;
        std::function<void()> restartSystem;
        std::function<state_machine::ControlMode()> getActiveMode;
        std::function<bool()> wasCommandRejectedSent;
        std::function<bool()> wasSelectResponseSent;
        std::function<services::FocRejectReason()> lastCommandRejectedReason;
        std::function<services::FocRejectReason()> lastSelectResponseReason;
        std::function<int()> nvmWriteCount;

        bool calibrationExpectationsConfigured{ false };

        std::function<void(std::size_t)> completePolePairsEstimation;
        std::function<void(foc::Ohm, foc::MilliHenry)> completeRLEstimation;
        std::function<void(foc::Radians)> completeAlignment;
        std::function<void()> completePolePairsFailure;
        std::function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> completeMechanicalIdentification;
    };
}
