#include "tools/can_commander/logic/MotorSettings.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "generated/Boards.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace tool
{
    namespace
    {
        using can::FocControlLoop;
        using can::FocParameter;

        template<class Algorithm>
        constexpr uint32_t Uses(std::initializer_list<Algorithm> algorithms)
        {
            uint32_t mask{ 0 };
            for (const auto algorithm : algorithms)
                mask |= 1u << static_cast<unsigned>(algorithm);
            return mask;
        }

        using Current = can::FocCurrentAlgorithm;
        using Speed = can::FocSpeedAlgorithm;
        using Position = can::FocPositionAlgorithm;

        constexpr std::string_view radPerSecond{ " rad/s" };
        constexpr std::string_view radPerSecondSquared{ " rad/s\xC2\xB2" };
        constexpr std::string_view ohm{ " \xCE\xA9" };
        constexpr std::string_view kilogramSquareMetre{ " kg\xC2\xB7m\xC2\xB2" };
        constexpr std::string_view frictionUnit{ " N\xC2\xB7m\xC2\xB7s/rad" };
        constexpr std::string_view newtonMetre{ " N\xC2\xB7m" };
        constexpr std::string_view cycles{ " cycles" };

        constexpr std::array<SettingPresentation, 89> presentations{ {
            { FocParameter::currentBandwidth, "Current loop bandwidth", "Bandwidth:", radPerSecond,
                "Closed-loop bandwidth of the current loop; deadbeat estimates its voltage error at this bandwidth. Automatic takes it from the calibration record, or from the PWM rate without one, held low enough that the current-sensor noise the calibration measured stays inaudible.",
                "Automatic", Uses({ Current::pid, Current::decoupledPid, Current::deadbeat, Current::slidingMode }) },
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
            { FocParameter::fluxLinkage, "Flux linkage", "Flux linkage:", " Wb", "Permanent-magnet flux linkage. Calibrate measures it; Platform default uses the value built into the firmware.", "Platform default" },
            { FocParameter::inertia, "Inertia", "Inertia:", kilogramSquareMetre, "Inertia of the rotor and its load.", {} },
            { FocParameter::viscousFriction, "Viscous friction", "Viscous friction:", frictionUnit, "Viscous friction coefficient of the rotor and its load.", {} },
            { FocParameter::frictionCoulomb, "Coulomb friction", "Coulomb friction:", newtonMetre, "Dry friction measured by Calibrate in speed and position mode, fed forward to the current reference.", {} },
            { FocParameter::encoderResolution, "Encoder resolution", "Encoder resolution:", " counts/rev", "Encoder counts per mechanical revolution.", {} },
            { FocParameter::canNodeId, "CAN node ID base", "CAN node ID base:", {}, "Base of the controller's CAN node ID; the board ID set by its jumpers is added to it.", {} },
            { FocParameter::canBitrate, "CAN bitrate", "CAN bitrate:", " bit/s", "Bitrate of the CAN bus.", {} },
            { FocParameter::telemetryRate, "Current stream rate", "Current stream rate:", " Hz", "How often the controller sends the Id, Iq and position samples the Telemetry panel plots, while this tool is connected and a control loop runs; 0 stops the stream. Every node's stream shares the bus, so the rates of all nodes together have to fit its bitrate.", {} },
            { FocParameter::supervisionWindow, "Supervision window", "Window:", " s", "How long the encoder reading has to stay still while the drive pushes for motion before a fault is raised.", {} },
            { FocParameter::supervisionCurrentFraction, "Supervision current fraction", "Current fraction:", {}, "Torque current, as a fraction of the drive's current limit, from which the drive counts as pushing.", {} },
            { FocParameter::supervisionDemandedSpeed, "Supervision demanded speed", "Demanded speed:", radPerSecond, "Speed reference from which the drive counts as demanding motion.", {} },
            { FocParameter::supervisionPositionError, "Supervision position error", "Position error:", " rad", "Position error from which the drive counts as demanding motion. Above 3.14 rad only a speed reference counts.", {} },
            { FocParameter::supervisionStillExcursion, "Supervision still excursion", "Still excursion:", " rad", "Travel within which the encoder reading counts as still.", {} },
            { FocParameter::stallDetection, "Stall reporting", "Report stalls:", {}, "Raise a stall fault when an outside force pushes the rotor against the drive and holds it there. Encoder loss is reported either way.", {} },
            { FocParameter::overspeedLimit, "Overspeed limit", "Overspeed limit:", radPerSecond, "Speed the rotor is kept below. Torque mode folds its current to zero over the last tenth of the limit, speed mode holds a setpoint beyond it at the limit, and every mode faults with overspeed above 110 % of it.", {} },
            { FocParameter::followingErrorWindow, "Following-error window", "Following-error window:", " rad", "Position error that position mode may not exceed for longer than the following-error time; 0 turns the check off. Set it above the largest position step you command.", {} },
            { FocParameter::followingErrorTime, "Following-error time", "Following-error time:", " s", "How long the position error may stay beyond the following-error window before the drive faults.", {} },
            { FocParameter::speedAcceleration, "Speed acceleration", "Acceleration:", radPerSecondSquared, "Largest rate at which speed mode lets its speed reference grow towards the setpoint; 0 leaves it unlimited.", {} },
            { FocParameter::speedDeceleration, "Speed deceleration", "Deceleration:", radPerSecondSquared, "Largest rate at which speed mode lets its speed reference shrink towards the setpoint, also through a reversal; 0 leaves it unlimited.", {} },
            { FocParameter::commandTimeout, "Command timeout", "Command timeout:", " s", "Longest a running motor may go without a Start or a setpoint over CAN before it faults; 0 turns the check off. This tool sends a setpoint only when you change it, so with a timeout set a motor it starts faults once the timeout runs out.", {} },
            { FocParameter::continuousCurrent, "Continuous current", "Continuous current:", " A", "Current the motor can carry for ever. Once a thermal model of the winding says the motor has carried more for too long, the current limit folds back to it; 0 turns the model off.", {} },
            { FocParameter::peakCurrent, "Peak current", "Peak current:", " A", "Current a cold motor may carry for the peak time; 0 means the board's current limit, which also caps it.", {} },
            { FocParameter::peakCurrentTime, "Peak time", "Peak time:", " s", "How long a cold motor may carry its peak current before the limit folds back to the continuous current.", {} },
            { FocParameter::profileVelocity, "Profile velocity", "Velocity:", radPerSecond, "Fastest speed a position move reaches. Position mode plans a trapezoidal path to each setpoint within the profile velocity, acceleration and deceleration; 0 in the velocity or the acceleration turns the profile off and the position loop is given the setpoint at once.", {} },
            { FocParameter::profileAcceleration, "Profile acceleration", "Acceleration:", radPerSecondSquared, "Largest rate at which a position move speeds up; 0 turns the profile off.", {} },
            { FocParameter::profileDeceleration, "Profile deceleration", "Deceleration:", radPerSecondSquared, "Largest rate at which a position move slows down; 0 uses the profile acceleration.", {} },
            { FocParameter::profileJerk, "Profile jerk", "Jerk:", " rad/s\xC2\xB3", "Reserved for a later S-curve profile: it is stored and read back, but the trapezoidal profile does not use it yet.", {} },
            { FocParameter::positionWindow, "Position window", "Position window:", " rad", "Position error within which position mode counts its target as reached; 0 keeps the target reported not reached.", {} },
            { FocParameter::positionWindowTime, "Position window time", "Window time:", " s", "How long the position error has to stay within the position window before the target is reported reached.", {} },
            { FocParameter::positionMinimum, "Position minimum", "Minimum:", " rad", "Lowest position, from the alignment zero, position mode accepts and moves to. The limits are off while the minimum is not below the maximum.", {} },
            { FocParameter::positionMaximum, "Position maximum", "Maximum:", " rad", "Highest position, from the alignment zero, position mode accepts and moves to. Between the limits the motor moves the direct way, never across the angle outside them.", {} },
            { FocParameter::calibrationTemperature, "Calibration temperature", "Calibration temperature:", " \xC2\xB0"
                                                                                                           "C",
                "Winding temperature when the calibration measured the resistance; the winding-temperature estimate starts from it.", {} },
            { FocParameter::temperatureLimit, "Temperature limit", "Temperature limit:", " \xC2\xB0"
                                                                                         "C",
                "Winding temperature at which speed and position mode fault with over-temperature, from 5 s after the motor starts.", {} },
            { FocParameter::temperatureMargin, "Temperature margin", "Derating margin:", " K", "Span below the temperature limit over which the current limit falls linearly to zero.", {} },
            { FocParameter::busVoltageMinimum, "Bus voltage minimum", "Bus minimum:", " V", "Bus voltage below which a running motor faults with undervoltage; 0 turns the check off.", {} },
            { FocParameter::busVoltageMaximum, "Bus voltage maximum", "Bus maximum:", " V", "Bus voltage above which a running motor faults with overvoltage, as when braking feeds the supply; 0 turns the check off.", {} },
            { FocParameter::currentOffsetA, "Phase A current offset", "Phase A offset:", " A", "Current-sensor offset of phase A in the calibration record.", {} },
            { FocParameter::currentOffsetB, "Phase B current offset", "Phase B offset:", " A", "Current-sensor offset of phase B in the calibration record.", {} },
            { FocParameter::currentOffsetC, "Phase C current offset", "Phase C offset:", " A", "Current-sensor offset of phase C in the calibration record.", {} },
            { FocParameter::coggingPeak, "Cogging torque peak", "Cogging peak:", newtonMetre, "Peak of the stored cogging map; 0 when the record holds none.", {} },
            { FocParameter::estimatedResistance, "Estimated resistance", "Resistance:", ohm, "Online estimate of the phase resistance; 0 in torque mode, which runs no estimator.", {} },
            { FocParameter::estimatedInductance, "Estimated inductance", "Inductance:", " mH", "Online estimate of the stator inductance; 0 in torque mode, which runs no estimator.", {} },
            { FocParameter::estimatedInertia, "Estimated inertia", "Inertia:", kilogramSquareMetre, "Online estimate of the inertia of the rotor and its load; 0 in torque mode, which runs no estimator.", {} },
            { FocParameter::estimatedViscousFriction, "Estimated viscous friction", "Viscous friction:", frictionUnit, "Online estimate of the viscous friction coefficient; 0 in torque mode, which runs no estimator.", {} },
            { FocParameter::loopSamples, "Control loop samples", "Samples:", {}, "Control periods measured since the statistics were last cleared.", {} },
            { FocParameter::loopBudgetCycles, "Control loop budget", "Budget:", cycles, "Cycles one control period allows.", {} },
            { FocParameter::loopMinimumCycles, "Shortest control loop run", "Minimum:", cycles, "Shortest control-interrupt execution.", {} },
            { FocParameter::loopAverageCycles, "Average control loop run", "Average:", cycles, "Mean control-interrupt execution.", {} },
            { FocParameter::loopMaximumCycles, "Longest control loop run", "Maximum:", cycles, "Longest control-interrupt execution.", {} },
            { FocParameter::loopOverruns, "Control loop overruns", "Overruns:", {}, "Executions longer than the budget.", {} },
            { FocParameter::loopDeadlineMisses, "Control loop deadline misses", "Deadline misses:", {}, "Periods whose interrupt started late.", {} },
            { FocParameter::loopReentries, "Control loop re-entries", "Re-entries:", {}, "Interrupts that arrived while the previous one still ran.", {} },
            { FocParameter::resetCause, "Reset cause", "Reset cause:", {}, "Cause of the last reset of the controller.", {} },
            { FocParameter::firmwareVersionMajor, "Firmware major version", "Firmware major:", {}, "Major number of the release version of the running firmware.", {} },
            { FocParameter::firmwareVersionMinor, "Firmware minor version", "Firmware minor:", {}, "Minor number of the release version of the running firmware.", {} },
            { FocParameter::firmwareVersionPatch, "Firmware patch version", "Firmware patch:", {}, "Patch number of the release version of the running firmware.", {} },
            { FocParameter::boardKind, "Board", "Board:", {}, "Board the controller runs on; emulated is the QEMU software-in-the-loop target, virtual a virtual motor on this machine.", {} },
            { FocParameter::stagedVersionMajor, "Staged major version", "Staged major:", {}, "Major number of the release version of the pack staged for an update; 0 when none is staged.", {} },
            { FocParameter::stagedVersionMinor, "Staged minor version", "Staged minor:", {}, "Minor number of the release version of the pack staged for an update; 0 when none is staged.", {} },
            { FocParameter::stagedVersionPatch, "Staged patch version", "Staged patch:", {}, "Patch number of the release version of the pack staged for an update; 0 when none is staged.", {} },
            { FocParameter::targetReached, "Target reached", "Target reached:", {}, "Whether position mode's error has stayed within the position window for the window time. Read with every status.", {} },
            { FocParameter::windingTemperature, "Winding temperature", "Winding temperature:", " \xC2\xB0"
                                                                                               "C",
                "Winding temperature inferred from the online resistance estimate in speed and position mode; unknown in torque mode and without a calibration. Read with every status.", {} },
            { FocParameter::homed, "Homed", "Homed:", {}, "Whether the position zero is the one the drive was commissioned with: yes after a calibration or a reference restored from the encoder index in this power-up, no after a plain alignment or at power-up. Read with every status.", {} },
            { FocParameter::recalibrationMode, "Online recalibration", "Recalibration:", {}, "When settled online estimates may replace the model the speed and position loops are designed from: Off never, On command on Recalibrate (Diagnostics tab), Automatic at every stop.", {} },
            { FocParameter::modelResistance, "Resistance in use", "Resistance:", ohm, "Resistance the current loop is designed from; the calibration record's until a recalibration.", {} },
            { FocParameter::modelInductance, "Inductance in use", "Inductance:", " mH", "Inductance the current loop is designed from; the calibration record's until a recalibration.", {} },
            { FocParameter::modelInertia, "Inertia in use", "Inertia:", kilogramSquareMetre, "Inertia the speed loop is designed from; the calibration record's until a recalibration.", {} },
            { FocParameter::modelViscousFriction, "Viscous friction in use", "Viscous friction:", frictionUnit, "Viscous friction the speed loop is designed from; the calibration record's until a recalibration.", {} },
            { FocParameter::recalibrationsApplied, "Recalibrations applied", "Applied:", {}, "Quantities recalibrated since power-up.", {} },
            { FocParameter::recalibrationsRejected, "Recalibrations rejected", "Rejected:", {}, "Settled readings outside their plausibility band since power-up.", {} },
            { FocParameter::recalibrationsStored, "Recalibrations stored", "Stored:", {}, "Stores of the recalibrated inductance, inertia and friction since power-up, at most one every 15 minutes.", {} },
            { FocParameter::homingRequired, "Homing required", "Require homing:", {}, "Refuse to enable position mode until the drive is homed, that is until a calibration or a reference restored from the encoder index has defined the commissioned position zero in this power-up. Speed and torque mode are never held back.", {} },
        } };

        constexpr bool MatchesWireContract()
        {
            if (presentations.size() != can::focParameters.size())
                return false;

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
            return std::ranges::find_if(changes, [parameter](const SettingChange& change)
                {
                    return change.kind == SettingChange::Kind::parameter && change.parameter == parameter;
                });
        }

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
        return Slice(14, 7);
    }

    std::span<const SettingPresentation> StartupSettings()
    {
        return Slice(21, 3);
    }

    std::span<const SettingPresentation> StreamSettings()
    {
        return Slice(24, 1);
    }

    std::span<const SettingPresentation> SupervisionSettings()
    {
        return Slice(25, 6);
    }

    std::span<const SettingPresentation> LimitSettings()
    {
        return Slice(31, 9);
    }

    std::span<const SettingPresentation> MotionProfileSettings()
    {
        return Slice(40, 4);
    }

    std::span<const SettingPresentation> TargetWindowSettings()
    {
        return Slice(44, 2);
    }

    std::span<const SettingPresentation> TravelLimitSettings()
    {
        return Slice(46, 2);
    }

    std::span<const SettingPresentation> ThermalSettings()
    {
        return Slice(48, 1);
    }

    std::span<const SettingPresentation> OverTemperatureSettings()
    {
        return Slice(49, 2);
    }

    std::span<const SettingPresentation> BusVoltageSettings()
    {
        return Slice(51, 2);
    }

    std::span<const SettingPresentation> Readings()
    {
        return Slice(53, 24);
    }

    std::span<const SettingPresentation> StatusReadings()
    {
        return Slice(77, 3);
    }

    std::span<const SettingPresentation> RecalibrationSettings()
    {
        return Slice(80, 1);
    }

    std::span<const SettingPresentation> RecalibrationReadings()
    {
        return Slice(81, 7);
    }

    std::span<const SettingPresentation> HomingSettings()
    {
        return Slice(88, 1);
    }

    namespace
    {
        std::string_view NameAt(std::span<const std::string_view> names, float value)
        {
            const auto index = std::lround(value);
            if (index < 0 || static_cast<std::size_t>(index) >= names.size() || static_cast<float>(index) != value)
                return "Unknown";
            return names[static_cast<std::size_t>(index)];
        }
    }

    std::span<const std::string_view> RecalibrationModes()
    {
        static constexpr std::array<std::string_view, 3> names{ "Off", "On command", "Automatic" };
        return names;
    }

    std::string_view RecalibrationModeName(float mode)
    {
        return NameAt(RecalibrationModes(), mode);
    }

    std::string_view ResetCauseName(float cause)
    {
        static constexpr std::array<std::string_view, 5> names{ "Power-up", "Brown-out", "Software", "Hardware", "Watchdog" };
        return NameAt(names, cause);
    }

    std::string_view BoardKindName(float kind)
    {
        const auto board = std::ranges::find_if(e_foc::boards::all, [kind](const e_foc::boards::Board& candidate)
            {
                return static_cast<float>(candidate.kind) == kind;
            });
        return board != e_foc::boards::all.end() ? board->name : "Unknown";
    }

    const SettingPresentation& PresentationOf(can::FocParameter parameter)
    {
        const auto presentation = std::ranges::find_if(presentations, [parameter](const SettingPresentation& entry)
            {
                return entry.parameter == parameter;
            });
        return presentation != presentations.end() ? *presentation : presentations.front();
    }

    bool IsUsedBy(const SettingPresentation& setting, uint8_t algorithm)
    {
        return algorithm < std::numeric_limits<uint32_t>::digits && (setting.usedBy & (1u << algorithm)) != 0;
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

    bool IsToggle(can::FocParameter parameter)
    {
        const auto descriptor = can::FindParameter(parameter);
        return descriptor.has_value() && descriptor->integer && descriptor->minimum == 0.0f && descriptor->maximum == 1.0f;
    }

    bool IsAcceptableEntry(can::FocParameter parameter, float value)
    {
        const auto descriptor = can::FindParameter(parameter);
        if (!descriptor.has_value() || (descriptor->automatic && value == 0.0f))
            return false;

        return can::IsAcceptableParameterValue(*descriptor, value);
    }

    uint8_t CommandTypeOf(const SettingChange& change)
    {
        if (change.kind == SettingChange::Kind::algorithm)
            return can::focSelectAlgorithmId;

        return can::focSetParameterId;
    }

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
