#include "tools/can_commander/logic/MotorSettings.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace tool
{
    namespace
    {
        using can::FocControlLoop;
        using can::FocParameter;

        template<class Algorithm>
        constexpr uint8_t Uses(std::initializer_list<Algorithm> algorithms)
        {
            uint8_t mask{ 0 };
            for (const auto algorithm : algorithms)
                mask = static_cast<uint8_t>(mask | (1u << static_cast<unsigned>(algorithm)));
            return mask;
        }

        using Current = can::FocCurrentAlgorithm;
        using Speed = can::FocSpeedAlgorithm;
        using Position = can::FocPositionAlgorithm;

        // Units are spelled as UTF-8 escapes so the bytes survive any compiler source code page
        constexpr std::string_view radPerSecond{ " rad/s" };
        constexpr std::string_view ohm{ " \xCE\xA9" };
        constexpr std::string_view kilogramSquareMetre{ " kg\xC2\xB7m\xC2\xB2" };
        constexpr std::string_view frictionUnit{ " N\xC2\xB7m\xC2\xB7s/rad" };

        constexpr std::array<SettingPresentation, 23> presentations{ {
            { FocParameter::currentBandwidth, "Current loop bandwidth", "Bandwidth:", radPerSecond,
                "Closed-loop bandwidth of the current loop. Automatic takes it from the calibration record, or from the PWM rate without one.",
                "Automatic", Uses({ Current::pid, Current::decoupledPid, Current::slidingMode }) },
            { FocParameter::currentSwitchingGain, "Current loop switching gain", "Switching gain:", " A",
                "Switching gain of the sliding-mode law. It must stay below the boundary layer.", {}, Uses({ Current::slidingMode }) },
            { FocParameter::currentBoundaryLayer, "Current loop boundary layer", "Boundary layer:", " A",
                "Width of the band in which the sliding-mode law acts linearly. It must stay above the switching gain.", {}, Uses({ Current::slidingMode }) },
            { FocParameter::currentTwoStepDeadbeat, "Deadbeat two-step variant", "Two-step variant:", {},
                "Use the two-step variant of the deadbeat law.", {}, Uses({ Current::deadbeat }) },
            { FocParameter::speedBandwidth, "Speed loop bandwidth", "Bandwidth:", radPerSecond,
                "Closed-loop bandwidth of the speed loop. Automatic takes it from the calibration record, or 50 rad/s without one.",
                "Automatic", Uses({ Speed::pid, Speed::lqi, Speed::adrc, Speed::twoDof }) },
            { FocParameter::speedErrorWeight, "Speed loop error weight", "Speed error weight:", {},
                "LQI weight on the speed error.", {}, Uses({ Speed::lqi }) },
            { FocParameter::speedIntegralWeight, "Speed loop integral weight", "Integral weight:", {},
                "LQI weight on the integrated speed error.", {}, Uses({ Speed::lqi }) },
            { FocParameter::speedObserverBandwidthRatio, "Speed loop observer bandwidth ratio", "Observer bandwidth ratio:", {},
                "Bandwidth of the ADRC extended state observer, as a multiple of the loop bandwidth.", {}, Uses({ Speed::adrc }) },
            { FocParameter::speedReferenceTimeConstant, "Speed loop reference time constant", "Reference time constant:", " s",
                "Time constant of the two-DOF reference pre-filter.", {}, Uses({ Speed::twoDof }) },
            { FocParameter::positionBandwidth, "Position loop bandwidth", "Bandwidth:", radPerSecond,
                "Closed-loop bandwidth of the position loop. Automatic is 3 Hz (18.85 rad/s).",
                "Automatic", Uses({ Position::pid, Position::cascadeP, Position::lqr, Position::lqi, Position::twoDof }) },
            { FocParameter::positionErrorWeight, "Position loop error weight", "Position error weight:", {},
                "LQR and LQI weight on the position error.", {}, Uses({ Position::lqr, Position::lqi }) },
            { FocParameter::positionSpeedErrorWeight, "Position loop speed error weight", "Speed error weight:", {},
                "LQR and LQI weight on the speed error.", {}, Uses({ Position::lqr, Position::lqi }) },
            { FocParameter::positionIntegralWeight, "Position loop integral weight", "Integral weight:", {},
                "LQI weight on the integrated position error.", {}, Uses({ Position::lqi }) },
            { FocParameter::positionReferenceTimeConstant, "Position loop reference time constant", "Reference time constant:", " s",
                "Time constant of the two-DOF reference pre-filter.", {}, Uses({ Position::twoDof }) },
            { FocParameter::resistance, "Resistance", "Resistance:", ohm, "Stator phase resistance.", {} },
            { FocParameter::inductance, "Inductance", "Inductance:", " mH", "Stator inductance, used for both axes.", {} },
            { FocParameter::polePairs, "Pole pairs", "Pole pairs:", {}, "Rotor pole pairs. After a change the rotor has to be aligned again before the motor can start.", {} },
            { FocParameter::fluxLinkage, "Flux linkage", "Flux linkage:", " Wb", "Permanent-magnet flux linkage. Platform default uses the value built into the firmware.", "Platform default" },
            { FocParameter::inertia, "Inertia", "Inertia:", kilogramSquareMetre, "Inertia of the rotor and its load.", {} },
            { FocParameter::viscousFriction, "Viscous friction", "Viscous friction:", frictionUnit, "Viscous friction coefficient of the rotor and its load.", {} },
            { FocParameter::encoderResolution, "Encoder resolution", "Encoder resolution:", " counts/rev", "Encoder counts per mechanical revolution.", {} },
            { FocParameter::canNodeId, "CAN node ID base", "CAN node ID base:", {}, "Base of the controller's CAN node ID; the board ID set by its jumpers is added to it.", {} },
            { FocParameter::canBitrate, "CAN bitrate", "CAN bitrate:", " bit/s", "Bitrate of the CAN bus.", {} },
        } };

        constexpr bool MatchesWireContract()
        {
            for (std::size_t i = 0; i != presentations.size(); ++i)
                if (presentations[i].parameter != can::focParameters[i].id)
                    return false;

            return true;
        }

        static_assert(MatchesWireContract(), "one presentation per wire-contract parameter, in the same order");

        constexpr std::array<FocControlLoop, 3> allLoops{ FocControlLoop::current, FocControlLoop::speed, FocControlLoop::position };

        constexpr std::span<const SettingPresentation> Slice(std::size_t first, std::size_t count)
        {
            return std::span<const SettingPresentation>{ presentations }.subspan(first, count);
        }

        std::vector<SettingChange>::iterator Find(std::vector<SettingChange>& changes, FocParameter parameter)
        {
            return std::find_if(changes.begin(), changes.end(), [parameter](const SettingChange& change)
                {
                    return change.kind == SettingChange::Kind::parameter && change.parameter == parameter;
                });
        }

        // The firmware refuses a switching gain at or above the boundary layer, after every single write
        void OrderSlidingModePair(std::vector<SettingChange>& settings, std::optional<float> heldSwitchingGain)
        {
            const auto gain = Find(settings, FocParameter::currentSwitchingGain);
            const auto boundary = Find(settings, FocParameter::currentBoundaryLayer);
            if (gain == settings.end() || boundary == settings.end())
                return;

            const bool boundaryFirst = heldSwitchingGain.value_or(gain->value) < boundary->value;
            if (boundaryFirst != (boundary < gain))
                std::iter_swap(gain, boundary);
        }

        std::string_view SlidingModeProblem(std::vector<SettingChange>& settings, const LoopEdits& edits)
        {
            const auto gain = Find(settings, FocParameter::currentSwitchingGain);
            const auto boundary = Find(settings, FocParameter::currentBoundaryLayer);
            if (gain == settings.end() && boundary == settings.end())
                return {};

            const auto finalGain = gain != settings.end() ? std::optional<float>{ gain->value } : edits.heldSwitchingGain;
            const auto finalBoundary = boundary != settings.end() ? std::optional<float>{ boundary->value } : edits.heldBoundaryLayer;
            if (finalGain.has_value() && finalBoundary.has_value() && !(*finalGain < *finalBoundary))
                return "The switching gain must stay below the boundary layer.";

            return {};
        }
    }

    SettingChange SettingChange::Parameter(can::FocParameter id, float newValue)
    {
        SettingChange change;
        change.kind = Kind::parameter;
        change.parameter = id;
        change.value = newValue;
        return change;
    }

    SettingChange SettingChange::Algorithm(can::FocControlLoop controlLoop, uint8_t index)
    {
        SettingChange change;
        change.kind = Kind::algorithm;
        change.loop = controlLoop;
        change.algorithm = index;
        return change;
    }

    std::span<const can::FocControlLoop> LoopsOf(can::FocMotorMode mode)
    {
        switch (mode)
        {
            case can::FocMotorMode::torque:
                return std::span<const FocControlLoop>{ allLoops }.first(1);
            case can::FocMotorMode::speed:
                return std::span<const FocControlLoop>{ allLoops }.first(2);
            default:
                return allLoops;
        }
    }

    std::string_view LoopName(can::FocControlLoop loop)
    {
        switch (loop)
        {
            case FocControlLoop::current:
                return "Current Loop";
            case FocControlLoop::speed:
                return "Speed Loop";
            default:
                return "Position Loop";
        }
    }

    std::string_view AlgorithmName(can::FocControlLoop loop, uint8_t algorithm)
    {
        static constexpr std::array<std::string_view, 4> current{ "PID", "Decoupled PID", "Deadbeat", "Sliding mode" };
        static constexpr std::array<std::string_view, 4> speed{ "PID", "LQI", "ADRC", "Two-DOF" };
        static constexpr std::array<std::string_view, 5> position{ "PID", "Cascade P", "LQR", "LQI", "Two-DOF" };

        if (algorithm >= can::AlgorithmCount(loop))
            return "Unknown";

        switch (loop)
        {
            case FocControlLoop::current:
                return current[algorithm];
            case FocControlLoop::speed:
                return speed[algorithm];
            default:
                return position[algorithm];
        }
    }

    uint8_t AlgorithmOf(const can::FocAlgorithms& algorithms, can::FocControlLoop loop)
    {
        switch (loop)
        {
            case FocControlLoop::current:
                return static_cast<uint8_t>(algorithms.current);
            case FocControlLoop::speed:
                return static_cast<uint8_t>(algorithms.speed);
            default:
                return static_cast<uint8_t>(algorithms.position);
        }
    }

    // The laws whose design the firmware solves from the settings, and which can therefore refuse a setting
    bool RefusesSettings(can::FocControlLoop loop, uint8_t algorithm)
    {
        if (loop == FocControlLoop::speed)
            return algorithm == static_cast<uint8_t>(Speed::lqi);
        if (loop == FocControlLoop::position)
            return algorithm == static_cast<uint8_t>(Position::lqr) || algorithm == static_cast<uint8_t>(Position::lqi);
        return false;
    }

    std::span<const SettingPresentation> LoopSettings(can::FocControlLoop loop)
    {
        switch (loop)
        {
            case FocControlLoop::current:
                return Slice(0, 4);
            case FocControlLoop::speed:
                return Slice(4, 5);
            default:
                return Slice(9, 5);
        }
    }

    std::span<const SettingPresentation> MotorParameterSettings()
    {
        return Slice(14, 6);
    }

    std::span<const SettingPresentation> StartupSettings()
    {
        return Slice(20, 3);
    }

    const SettingPresentation& PresentationOf(can::FocParameter parameter)
    {
        const auto presentation = std::find_if(presentations.begin(), presentations.end(), [parameter](const SettingPresentation& entry)
            {
                return entry.parameter == parameter;
            });
        return presentation != presentations.end() ? *presentation : presentations.front();
    }

    bool IsUsedBy(const SettingPresentation& setting, uint8_t algorithm)
    {
        return algorithm < 8 && (setting.usedBy & (1u << algorithm)) != 0;
    }

    bool IsBandwidth(can::FocParameter parameter)
    {
        return parameter == FocParameter::currentBandwidth || parameter == FocParameter::speedBandwidth || parameter == FocParameter::positionBandwidth;
    }

    bool IsInteger(can::FocParameter parameter)
    {
        const auto descriptor = can::FindParameter(parameter);
        return descriptor.has_value() && descriptor->integer;
    }

    // Automatic is chosen explicitly, so a typed zero is refused rather than read as automatic; the
    // 2.0 bandwidth commands carry whole rad/s
    bool IsAcceptableEntry(can::FocParameter parameter, float value, bool legacy)
    {
        const auto descriptor = can::FindParameter(parameter);
        if (!descriptor.has_value() || (descriptor->automatic && value == 0.0f))
            return false;

        if (legacy && IsBandwidth(parameter) && std::round(value) != value)
            return false;

        return can::IsAcceptableParameterValue(*descriptor, value);
    }

    std::optional<uint8_t> LegacyCommandOf(can::FocParameter parameter)
    {
        switch (parameter)
        {
            case FocParameter::currentBandwidth:
                return can::focSetPidCurrentId;
            case FocParameter::speedBandwidth:
                return can::focSetPidSpeedId;
            case FocParameter::positionBandwidth:
                return can::focSetPidPositionId;
            case FocParameter::encoderResolution:
                return can::focSetEncoderResolutionId;
            default:
                return std::nullopt;
        }
    }

    uint8_t CommandTypeOf(const SettingChange& change, bool legacy)
    {
        if (change.kind == SettingChange::Kind::algorithm)
            return can::focSelectAlgorithmId;

        return legacy ? LegacyCommandOf(change.parameter).value_or(can::focSetParameterId) : can::focSetParameterId;
    }

    // A setting is checked against the law in force when it is written, and a law against the settings
    // stored when it is chosen. Settings go first, so the chosen law is designed with them; a law that
    // never refuses a setting goes first instead, so the law it replaces does not judge settings it
    // will no longer run.
    ChangePlan PlanLoop(const LoopEdits& edits)
    {
        auto settings = edits.settings;

        if (edits.loop == FocControlLoop::current)
        {
            if (const auto problem = SlidingModeProblem(settings, edits); !problem.empty())
                return ChangePlan{ {}, problem };

            OrderSlidingModePair(settings, edits.heldSwitchingGain);
        }

        ChangePlan plan;
        const bool algorithmChanges = edits.algorithm.has_value() && edits.algorithm != edits.heldAlgorithm;
        const bool algorithmFirst = algorithmChanges && !RefusesSettings(edits.loop, *edits.algorithm);

        if (algorithmFirst)
            plan.changes.push_back(SettingChange::Algorithm(edits.loop, *edits.algorithm));

        plan.changes.insert(plan.changes.end(), settings.begin(), settings.end());

        if (algorithmChanges && !algorithmFirst)
            plan.changes.push_back(SettingChange::Algorithm(edits.loop, *edits.algorithm));

        return plan;
    }

    void ChangeSequence::Start(std::vector<SettingChange> sequence)
    {
        changes = std::move(sequence);
        current = 0;
        active = !changes.empty();
    }

    void ChangeSequence::Stop()
    {
        active = false;
    }

    bool ChangeSequence::Advance()
    {
        if (!active)
            return false;

        ++current;
        active = current < changes.size();
        return active;
    }

    bool ChangeSequence::Active() const
    {
        return active;
    }

    const SettingChange& ChangeSequence::Current() const
    {
        return changes[current];
    }

    std::size_t ChangeSequence::Remaining() const
    {
        return active ? changes.size() - current : 0;
    }
}
