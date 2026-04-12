#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/software_in_the_loop/support/SpeedIntegrationFixture.hpp"
#include "integration_tests/software_in_the_loop/support/StateMachineAccessor.hpp"
#include <gtest/gtest.h>

using namespace testing;
using namespace integration;

static void PopulateSpeedAccessor(StateMachineAccessor& accessor, SpeedIntegrationFixture& fixture)
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

GIVEN(R"(the speed motor system is initialised with no valid calibration data)")
{
    context.Emplace<SpeedIntegrationFixture>();
    auto& fixture = context.Get<SpeedIntegrationFixture>();
    fixture.ConstructWithInvalidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulateSpeedAccessor(context.Get<StateMachineAccessor>(), fixture);
}

GIVEN(R"(the speed motor system is initialised with valid calibration data)")
{
    context.Emplace<SpeedIntegrationFixture>();
    auto& fixture = context.Get<SpeedIntegrationFixture>();
    fixture.ConstructWithValidNvm();

    context.Emplace<StateMachineAccessor>();
    PopulateSpeedAccessor(context.Get<StateMachineAccessor>(), fixture);
}

WHEN(R"(the mechanical identification completes successfully)")
{
    context.Get<StateMachineAccessor>().completeMechanicalIdentification(
        std::optional<foc::NewtonMeterSecondPerRadian>{ foc::NewtonMeterSecondPerRadian{ 0.01f } },
        std::optional<foc::NewtonMeterSecondSquared>{ foc::NewtonMeterSecondSquared{ 0.005f } });
}
