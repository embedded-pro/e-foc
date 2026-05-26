#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <functional>
#include <optional>

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
        std::function<void()> injectCanEmergencyStop;
        std::function<void()> triggerHardwareFault;

        // Async-race helpers: defer the NVM invalidation so state can change before it completes.
        std::function<void()> deferClearCalibration;
        std::function<void(services::NvmStatus)> completeInvalidate;

        bool calibrationExpectationsConfigured{ false };

        std::function<void(std::size_t)> completePolePairsEstimation;
        std::function<void(foc::Ohm, foc::MilliHenry)> completeRLEstimation;
        std::function<void(foc::Radians)> completeAlignment;
        std::function<void()> completePolePairsFailure;
        std::function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> completeMechanicalIdentification;
    };
}
