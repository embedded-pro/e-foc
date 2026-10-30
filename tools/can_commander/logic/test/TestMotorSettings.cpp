#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

namespace
{
    using namespace tool;
    using can::FocControlLoop;
    using can::FocParameter;

    class TestMotorSettings
        : public testing::Test
    {
    public:
        static uint8_t Raw(auto algorithm)
        {
            return static_cast<uint8_t>(algorithm);
        }

        static std::vector<FocParameter> ParametersOf(const ChangePlan& plan)
        {
            std::vector<FocParameter> parameters;
            for (const auto& change : plan.changes)
                if (change.kind == SettingChange::Kind::parameter)
                    parameters.push_back(change.parameter);
            return parameters;
        }

        static LoopEdits SlidingModeEdits(float heldGain, float heldBoundary)
        {
            LoopEdits edits;
            edits.loop = FocControlLoop::current;
            edits.heldSwitchingGain = heldGain;
            edits.heldBoundaryLayer = heldBoundary;
            return edits;
        }
    };
}

TEST_F(TestMotorSettings, presents_every_parameter_of_the_wire_contract)
{
    for (const auto& descriptor : can::focParameters)
    {
        const auto& presentation = PresentationOf(descriptor.id);
        EXPECT_EQ(descriptor.id, presentation.parameter) << descriptor.name;
        EXPECT_FALSE(presentation.name.empty()) << descriptor.name;
        EXPECT_FALSE(presentation.label.empty()) << descriptor.name;
        EXPECT_FALSE(presentation.description.empty()) << descriptor.name;
        EXPECT_EQ(descriptor.automatic, !presentation.automaticLabel.empty()) << descriptor.name;
    }
}

TEST_F(TestMotorSettings, splits_the_parameters_into_loops_motor_and_startup)
{
    EXPECT_EQ(4u, LoopSettings(FocControlLoop::current).size());
    EXPECT_EQ(5u, LoopSettings(FocControlLoop::speed).size());
    EXPECT_EQ(5u, LoopSettings(FocControlLoop::position).size());
    EXPECT_EQ(FocParameter::speedBandwidth, LoopSettings(FocControlLoop::speed).front().parameter);
    EXPECT_EQ(FocParameter::positionReferenceTimeConstant, LoopSettings(FocControlLoop::position).back().parameter);
    EXPECT_EQ(FocParameter::resistance, MotorParameterSettings().front().parameter);
    EXPECT_EQ(FocParameter::viscousFriction, MotorParameterSettings().back().parameter);
    EXPECT_EQ(FocParameter::encoderResolution, StartupSettings().front().parameter);
    EXPECT_EQ(FocParameter::canBitrate, StartupSettings().back().parameter);
    EXPECT_EQ(FocParameter::supervisionWindow, SupervisionSettings().front().parameter);
    EXPECT_EQ(FocParameter::stallDetection, SupervisionSettings().back().parameter);
    EXPECT_EQ(FocParameter::currentOffsetA, Readings().front().parameter);
    EXPECT_EQ(FocParameter::resetCause, Readings().back().parameter);
}

TEST_F(TestMotorSettings, offers_for_editing_only_what_the_controller_accepts_and_reads_out_the_rest)
{
    const auto readOnly = [](const SettingPresentation& setting)
    {
        const auto descriptor = can::FindParameter(setting.parameter);
        return descriptor.has_value() && descriptor->readOnly;
    };

    for (const auto loop : { FocControlLoop::current, FocControlLoop::speed, FocControlLoop::position })
        EXPECT_TRUE(std::ranges::none_of(LoopSettings(loop), readOnly));
    EXPECT_TRUE(std::ranges::none_of(MotorParameterSettings(), readOnly));
    EXPECT_TRUE(std::ranges::none_of(StartupSettings(), readOnly));
    EXPECT_TRUE(std::ranges::none_of(SupervisionSettings(), readOnly));
    EXPECT_TRUE(std::ranges::all_of(Readings(), readOnly));
    EXPECT_EQ(can::focParameters.size(), LoopSettings(FocControlLoop::current).size() + LoopSettings(FocControlLoop::speed).size() + LoopSettings(FocControlLoop::position).size() + MotorParameterSettings().size() + StartupSettings().size() + SupervisionSettings().size() + Readings().size());
}

TEST_F(TestMotorSettings, the_on_off_settings_are_toggles)
{
    EXPECT_TRUE(IsToggle(FocParameter::currentTwoStepDeadbeat));
    EXPECT_TRUE(IsToggle(FocParameter::stallDetection));
    EXPECT_FALSE(IsToggle(FocParameter::supervisionWindow));
    EXPECT_FALSE(IsToggle(FocParameter::polePairs));
    EXPECT_FALSE(IsToggle(FocParameter::resetCause));
}

TEST_F(TestMotorSettings, each_mode_runs_its_loops)
{
    EXPECT_EQ((std::vector<FocControlLoop>{ FocControlLoop::current }), std::vector<FocControlLoop>(LoopsOf(can::FocMotorMode::torque).begin(), LoopsOf(can::FocMotorMode::torque).end()));
    EXPECT_EQ(2u, LoopsOf(can::FocMotorMode::speed).size());
    EXPECT_EQ(FocControlLoop::speed, LoopsOf(can::FocMotorMode::speed).back());
    EXPECT_EQ(3u, LoopsOf(can::FocMotorMode::position).size());
    EXPECT_EQ(FocControlLoop::position, LoopsOf(can::FocMotorMode::position).back());
}

TEST_F(TestMotorSettings, names_every_algorithm_of_every_loop)
{
    EXPECT_EQ("Sliding mode", AlgorithmName(FocControlLoop::current, Raw(can::FocCurrentAlgorithm::slidingMode)));
    EXPECT_EQ("ADRC", AlgorithmName(FocControlLoop::speed, Raw(can::FocSpeedAlgorithm::adrc)));
    EXPECT_EQ("Cascade P", AlgorithmName(FocControlLoop::position, Raw(can::FocPositionAlgorithm::cascadeP)));
    EXPECT_EQ("Two-DOF", AlgorithmName(FocControlLoop::position, Raw(can::FocPositionAlgorithm::twoDof)));
    EXPECT_EQ("Unknown", AlgorithmName(FocControlLoop::speed, can::AlgorithmCount(FocControlLoop::speed)));

    for (const auto loop : LoopsOf(can::FocMotorMode::position))
        for (uint8_t algorithm = 0; algorithm != can::AlgorithmCount(loop); ++algorithm)
            EXPECT_NE("Unknown", AlgorithmName(loop, algorithm));
}

TEST_F(TestMotorSettings, reads_the_algorithm_of_each_loop)
{
    const can::FocAlgorithms algorithms{ can::FocMotorMode::position, can::FocCurrentAlgorithm::deadbeat, can::FocSpeedAlgorithm::lqi, can::FocPositionAlgorithm::lqr };

    EXPECT_EQ(Raw(can::FocCurrentAlgorithm::deadbeat), AlgorithmOf(algorithms, FocControlLoop::current));
    EXPECT_EQ(Raw(can::FocSpeedAlgorithm::lqi), AlgorithmOf(algorithms, FocControlLoop::speed));
    EXPECT_EQ(Raw(can::FocPositionAlgorithm::lqr), AlgorithmOf(algorithms, FocControlLoop::position));
}

TEST_F(TestMotorSettings, shows_each_setting_with_the_algorithms_that_use_it)
{
    const auto& currentBandwidth = PresentationOf(FocParameter::currentBandwidth);
    EXPECT_TRUE(IsUsedBy(currentBandwidth, Raw(can::FocCurrentAlgorithm::pid)));
    EXPECT_TRUE(IsUsedBy(currentBandwidth, Raw(can::FocCurrentAlgorithm::slidingMode)));
    EXPECT_TRUE(IsUsedBy(currentBandwidth, Raw(can::FocCurrentAlgorithm::deadbeat)));
    EXPECT_FALSE(IsUsedBy(PresentationOf(FocParameter::currentSwitchingGain), Raw(can::FocCurrentAlgorithm::deadbeat)));

    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::currentTwoStepDeadbeat), Raw(can::FocCurrentAlgorithm::deadbeat)));
    EXPECT_FALSE(IsUsedBy(PresentationOf(FocParameter::currentSwitchingGain), Raw(can::FocCurrentAlgorithm::decoupledPid)));

    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::speedIntegralWeight), Raw(can::FocSpeedAlgorithm::lqi)));
    EXPECT_FALSE(IsUsedBy(PresentationOf(FocParameter::speedIntegralWeight), Raw(can::FocSpeedAlgorithm::adrc)));
    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::speedObserverBandwidthRatio), Raw(can::FocSpeedAlgorithm::adrc)));

    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::positionSpeedErrorWeight), Raw(can::FocPositionAlgorithm::lqr)));
    EXPECT_FALSE(IsUsedBy(PresentationOf(FocParameter::positionIntegralWeight), Raw(can::FocPositionAlgorithm::lqr)));
    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::positionReferenceTimeConstant), Raw(can::FocPositionAlgorithm::twoDof)));
    EXPECT_FALSE(IsUsedBy(PresentationOf(FocParameter::positionErrorWeight), Raw(can::FocPositionAlgorithm::pid)));

    EXPECT_TRUE(IsUsedBy(PresentationOf(FocParameter::inertia), 0));
}

TEST_F(TestMotorSettings, every_algorithm_uses_the_settings_it_designs_from)
{
    for (const auto loop : LoopsOf(can::FocMotorMode::position))
        for (const auto& setting : LoopSettings(loop))
        {
            bool used{ false };
            for (uint8_t algorithm = 0; algorithm != can::AlgorithmCount(loop); ++algorithm)
                used = used || IsUsedBy(setting, algorithm);
            EXPECT_TRUE(used) << setting.name;
        }
}

TEST_F(TestMotorSettings, only_designed_laws_refuse_settings)
{
    EXPECT_TRUE(RefusesSettings(FocControlLoop::speed, Raw(can::FocSpeedAlgorithm::lqi)));
    EXPECT_TRUE(RefusesSettings(FocControlLoop::position, Raw(can::FocPositionAlgorithm::lqr)));
    EXPECT_TRUE(RefusesSettings(FocControlLoop::position, Raw(can::FocPositionAlgorithm::lqi)));
    EXPECT_FALSE(RefusesSettings(FocControlLoop::speed, Raw(can::FocSpeedAlgorithm::adrc)));
    EXPECT_FALSE(RefusesSettings(FocControlLoop::position, Raw(can::FocPositionAlgorithm::cascadeP)));
    EXPECT_FALSE(RefusesSettings(FocControlLoop::current, Raw(can::FocCurrentAlgorithm::slidingMode)));
}

TEST_F(TestMotorSettings, accepts_entries_within_the_firmware_range)
{
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::currentBandwidth, 3141.5f, false));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::currentBandwidth, 20001.0f, false));
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::inertia, 1.0e-9f, false));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::resistance, 0.0f, false));
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::viscousFriction, 0.0f, false));
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::polePairs, 7.0f, false));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::polePairs, 7.5f, false));
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::canBitrate, 500000.0f, false));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::canBitrate, 400000.0f, false));
}

TEST_F(TestMotorSettings, a_typed_zero_is_not_read_as_automatic)
{
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::speedBandwidth, 0.0f, false));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::fluxLinkage, 0.0f, false));
}

TEST_F(TestMotorSettings, the_legacy_bandwidth_commands_carry_whole_radians_per_second)
{
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::speedBandwidth, 188.0f, true));
    EXPECT_FALSE(IsAcceptableEntry(FocParameter::speedBandwidth, 188.5f, true));
    EXPECT_TRUE(IsAcceptableEntry(FocParameter::speedBandwidth, 188.5f, false));
}

TEST_F(TestMotorSettings, maps_settings_to_the_commands_that_carry_them)
{
    EXPECT_EQ(can::focSetPidSpeedId, LegacyCommandOf(FocParameter::speedBandwidth));
    EXPECT_EQ(can::focSetEncoderResolutionId, LegacyCommandOf(FocParameter::encoderResolution));
    EXPECT_FALSE(LegacyCommandOf(FocParameter::inertia).has_value());

    EXPECT_EQ(can::focSetParameterId, CommandTypeOf(SettingChange::Parameter(FocParameter::speedBandwidth, 100.0f), false));
    EXPECT_EQ(can::focSetPidSpeedId, CommandTypeOf(SettingChange::Parameter(FocParameter::speedBandwidth, 100.0f), true));
    EXPECT_EQ(can::focSelectAlgorithmId, CommandTypeOf(SettingChange::Algorithm(FocControlLoop::speed, 1), false));
}

TEST_F(TestMotorSettings, settings_go_before_a_law_that_is_designed_from_them)
{
    LoopEdits edits;
    edits.loop = FocControlLoop::speed;
    edits.heldAlgorithm = Raw(can::FocSpeedAlgorithm::pid);
    edits.algorithm = Raw(can::FocSpeedAlgorithm::lqi);
    edits.settings = { SettingChange::Parameter(FocParameter::speedErrorWeight, 2.0f), SettingChange::Parameter(FocParameter::speedIntegralWeight, 0.2f) };

    const auto plan = PlanLoop(edits);

    ASSERT_EQ(3u, plan.changes.size());
    EXPECT_TRUE(plan.problem.empty());
    EXPECT_EQ(FocParameter::speedErrorWeight, plan.changes[0].parameter);
    EXPECT_EQ(FocParameter::speedIntegralWeight, plan.changes[1].parameter);
    EXPECT_EQ(SettingChange::Kind::algorithm, plan.changes[2].kind);
    EXPECT_EQ(Raw(can::FocSpeedAlgorithm::lqi), plan.changes[2].algorithm);
    EXPECT_EQ(FocControlLoop::speed, plan.changes[2].loop);
}

TEST_F(TestMotorSettings, a_law_that_never_refuses_settings_goes_first)
{
    LoopEdits edits;
    edits.loop = FocControlLoop::position;
    edits.heldAlgorithm = Raw(can::FocPositionAlgorithm::lqr);
    edits.algorithm = Raw(can::FocPositionAlgorithm::pid);
    edits.settings = { SettingChange::Parameter(FocParameter::positionBandwidth, 30.0f) };

    const auto plan = PlanLoop(edits);

    ASSERT_EQ(2u, plan.changes.size());
    EXPECT_EQ(SettingChange::Kind::algorithm, plan.changes[0].kind);
    EXPECT_EQ(FocParameter::positionBandwidth, plan.changes[1].parameter);
}

TEST_F(TestMotorSettings, an_unchanged_law_is_not_selected_again)
{
    LoopEdits edits;
    edits.loop = FocControlLoop::speed;
    edits.heldAlgorithm = Raw(can::FocSpeedAlgorithm::adrc);
    edits.algorithm = Raw(can::FocSpeedAlgorithm::adrc);
    edits.settings = { SettingChange::Parameter(FocParameter::speedObserverBandwidthRatio, 8.0f) };

    const auto plan = PlanLoop(edits);

    ASSERT_EQ(1u, plan.changes.size());
    EXPECT_EQ(FocParameter::speedObserverBandwidthRatio, plan.changes[0].parameter);
}

TEST_F(TestMotorSettings, raising_the_sliding_mode_pair_writes_the_boundary_layer_first)
{
    auto edits = SlidingModeEdits(0.2f, 0.5f);
    edits.settings = { SettingChange::Parameter(FocParameter::currentSwitchingGain, 1.0f), SettingChange::Parameter(FocParameter::currentBoundaryLayer, 2.0f) };

    EXPECT_EQ((std::vector<FocParameter>{ FocParameter::currentBoundaryLayer, FocParameter::currentSwitchingGain }), ParametersOf(PlanLoop(edits)));
}

TEST_F(TestMotorSettings, lowering_the_sliding_mode_pair_writes_the_switching_gain_first)
{
    auto edits = SlidingModeEdits(1.0f, 2.0f);
    edits.settings = { SettingChange::Parameter(FocParameter::currentSwitchingGain, 0.1f), SettingChange::Parameter(FocParameter::currentBoundaryLayer, 0.3f) };

    EXPECT_EQ((std::vector<FocParameter>{ FocParameter::currentSwitchingGain, FocParameter::currentBoundaryLayer }), ParametersOf(PlanLoop(edits)));
}

TEST_F(TestMotorSettings, a_switching_gain_at_the_boundary_layer_is_refused_before_anything_is_sent)
{
    auto edits = SlidingModeEdits(0.2f, 0.5f);
    edits.settings = { SettingChange::Parameter(FocParameter::currentBandwidth, 3000.0f), SettingChange::Parameter(FocParameter::currentSwitchingGain, 0.5f) };

    const auto plan = PlanLoop(edits);

    EXPECT_TRUE(plan.changes.empty());
    EXPECT_FALSE(plan.problem.empty());
}

TEST_F(TestMotorSettings, a_boundary_layer_below_the_switching_gain_in_force_is_refused)
{
    auto edits = SlidingModeEdits(0.2f, 0.5f);
    edits.settings = { SettingChange::Parameter(FocParameter::currentBoundaryLayer, 0.1f) };

    EXPECT_FALSE(PlanLoop(edits).problem.empty());
}

TEST_F(TestMotorSettings, the_sliding_mode_pair_is_left_to_the_firmware_when_the_values_in_force_are_unknown)
{
    LoopEdits edits;
    edits.loop = FocControlLoop::current;
    edits.settings = { SettingChange::Parameter(FocParameter::currentBoundaryLayer, 0.1f) };

    const auto plan = PlanLoop(edits);

    EXPECT_TRUE(plan.problem.empty());
    EXPECT_EQ(1u, plan.changes.size());
}

TEST_F(TestMotorSettings, a_sequence_hands_out_one_change_at_a_time)
{
    ChangeSequence sequence;
    sequence.Start({ SettingChange::Parameter(FocParameter::resistance, 0.5f), SettingChange::Parameter(FocParameter::inductance, 1.2f) });

    ASSERT_TRUE(sequence.Active());
    EXPECT_EQ(FocParameter::resistance, sequence.Current().parameter);
    EXPECT_EQ(2u, sequence.Remaining());

    EXPECT_TRUE(sequence.Advance());
    EXPECT_EQ(FocParameter::inductance, sequence.Current().parameter);
    EXPECT_EQ(1u, sequence.Remaining());

    EXPECT_FALSE(sequence.Advance());
    EXPECT_FALSE(sequence.Active());
    EXPECT_EQ(0u, sequence.Remaining());
}

TEST_F(TestMotorSettings, a_stopped_sequence_sends_nothing_more)
{
    ChangeSequence sequence;
    sequence.Start({ SettingChange::Parameter(FocParameter::resistance, 0.5f), SettingChange::Parameter(FocParameter::inductance, 1.2f) });

    sequence.Stop();

    EXPECT_FALSE(sequence.Active());
    EXPECT_FALSE(sequence.Advance());
}

TEST_F(TestMotorSettings, an_empty_sequence_is_never_active)
{
    ChangeSequence sequence;
    sequence.Start({});

    EXPECT_FALSE(sequence.Active());
}

TEST_F(TestMotorSettings, names_the_reset_cause_the_controller_reports)
{
    EXPECT_EQ("Power-up", ResetCauseName(0.0f));
    EXPECT_EQ("Brown-out", ResetCauseName(1.0f));
    EXPECT_EQ("Software", ResetCauseName(2.0f));
    EXPECT_EQ("Hardware", ResetCauseName(3.0f));
    EXPECT_EQ("Watchdog", ResetCauseName(4.0f));
    EXPECT_EQ("Unknown", ResetCauseName(5.0f));
    EXPECT_EQ("Unknown", ResetCauseName(-1.0f));
    EXPECT_EQ("Unknown", ResetCauseName(2.5f));
}
