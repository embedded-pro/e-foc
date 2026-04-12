#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/PositionIntegrationFixture.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

static void PopulatePositionAccessor(StateMachineAccessor& accessor, PositionIntegrationFixture& fixture)
{
    accessor.stateMachine = &*fixture.motorStateMachine;
    accessor.executeAll = [&fixture]()
        {
            fixture.ExecuteAllActions();
        };
    accessor.setupCalibrationExpectations = [&fixture]()
        {
            fixture.SetupCalibrationExpectations();
        };
    accessor.setupCanIntegration = [&fixture]()
        {
            fixture.SetupCanIntegration();
        };
    accessor.injectCanStart = [&fixture]()
        {
            fixture.InjectCanStart();
        };
    accessor.injectCanStop = [&fixture]()
        {
            fixture.InjectCanStop();
        };
    accessor.injectCanClearFault = [&fixture]()
        {
            fixture.InjectCanClearFault();
        };
    accessor.triggerHardwareFault = [&fixture]()
        {
            fixture.faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
            fixture.ExecuteAllActions();
        };
    accessor.completePolePairsEstimation = [&fixture](std::size_t polePairs)
        {
            fixture.CompletePolePairsEstimation(polePairs);
        };
    accessor.completeRLEstimation = [&fixture](foc::Ohm r, foc::MilliHenry l)
        {
            fixture.CompleteRLEstimation(r, l);
        };
    accessor.completeAlignment = [&fixture](foc::Radians offset)
        {
            fixture.CompleteAlignment(offset);
        };
    accessor.completePolePairsFailure = [&fixture]()
        {
            fixture.capturedPolePairsCallback(std::nullopt);
            fixture.ExecuteAllActions();
        };
    accessor.completeMechanicalIdentification = [&fixture](
        std::optional<foc::NewtonMeterSecondPerRadian> friction,
        std::optional<foc::NewtonMeterSecondSquared> inertia)
        {
            fixture.CompleteMechanicalIdentification(friction, inertia);
        };
}

GIVEN(R"(the position motor system is initialised with no valid calibration data)")
{
    context.Emplace<PositionIntegrationFixture>();
    auto& fixture = context.Get<PositionIntegrationFixture>();
    fixture.ConstructWithInvalidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulatePositionAccessor(context.Get<StateMachineAccessor>(), fixture);
}

GIVEN(R"(the position motor system is initialised with valid calibration data)")
{
    context.Emplace<PositionIntegrationFixture>();
    auto& fixture = context.Get<PositionIntegrationFixture>();
    fixture.ConstructWithValidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulatePositionAccessor(context.Get<StateMachineAccessor>(), fixture);
}
