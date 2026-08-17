# Branch Audit: `refactor/foc-layering`

| Field | Value |
|---|---|
| Audit date | 2026-08-16 |
| Revision | 2 (re-audit; supersedes revision 1) |
| Branch | `refactor/foc-layering` |
| Reviewed head | `f8f427e` |
| Merge base | `b8903cf7` (`origin/main`) |
| Scope | 38 commits, 261 changed files |
| Verdict | **REQUEST CHANGES** |

## Method

Revision 2 re-audited the complete `origin/main...HEAD` diff independently of revision 1, then reconciled the two. Four parallel specialist passes covered control theory and numerics, runtime safety and lifecycle, documentation and traceability, and test/style/build hygiene. Every retained finding was re-verified against the current source, and the schema, link and traceability checks were re-run as executable scripts.

The review applied the repository rules for:

- Heap-free and deterministic embedded code
- FOC theory and controller equations
- Interrupt and callback safety
- State-machine lifecycle behavior
- Requirements, design, and theory consistency
- Test quality and traceability
- Documentation and agent-instruction consistency

Revision 1 findings are carried forward with an explicit verdict. Nothing was dropped silently.

## Reconciliation with Revision 1

| Rev 1 | Subject | Verdict | Change |
|---|---|---|---|
| C1 | PWM restarts after disable | Confirmed, strengthened | Root cause is broader: `Calculate()` never reads `enabled`, and writing duty cycles re-enables the PWM generator. Now C2. |
| C2 | Position mode unusable | Confirmed, both halves | Now C6. |
| C3 | Mode switch destroys pending-callback owner | Confirmed, downgraded | Currently narrow because the TI EEPROM read path completes synchronously. Now M2. |
| C4 | Low-priority callback lifetime | Confirmed | Now H1. |
| C5 | Hardware protection not wired | Confirmed | Now C5. |
| H1 | Current PI integral gain omits the sample period | Confirmed, escalated | The loop runs at 10 kHz, so the over-gain is about 10 000x rather than 20 000x. Now C7. |
| H2 | Sliding mode cannot hold a reference | Confirmed, quantified | Now H8. |
| H3 | Persisted algorithms applied before calibration | Confirmed, strengthened | The rejected choice is also erased from the stored configuration. Now H6. |
| H4 | Decoupled PID unselectable | Confirmed, strengthened | The dead flux-linkage term is the dominant back-EMF term, and deadbeat silently loses it too. Now H5. |
| H5 | Overlapping NVM wedges mode selection | Confirmed, escalated | Unserialized EEPROM access additionally trips an assertion and resets the MCU. Split into C3 and C4. |
| H6 | First-sample speed impulse | Confirmed | Now H2. |
| H7 | Requirements schema rejects `status` | Confirmed, quantified | Exactly two violations; the validation job fails. Now H11. |
| H8 | CAN acknowledgements are unconditional | Confirmed, strengthened | Also affects stop, clear-fault and emergency-stop; unvalidated setpoints added. Now H9. |
| M1 | Two-DOF position filter crosses the seam | Confirmed, strengthened | The prefilter also resets to zero instead of the current angle. Now M5. |
| M2 | Decoupled PI limits twice | Confirmed | Now M6. |
| M3 | LQI anti-windup one sample late | Confirmed | Now M7. |
| M4 | Virtual dispatch in the hot path | Confirmed, context corrected | The quoted budget headroom is itself wrong; see C8. Now M8. |
| M5 | Stale normative requirements | Confirmed | Now H12. |
| M6 | Selector design mixes implemented and deferred | Confirmed | Now M12. |
| M7 | CLI design drift | Confirmed, strengthened | No production CLI path can command a setpoint or bandwidth at all. Now H7. |
| M8 | Non-discriminating tests | Confirmed, greatly expanded | 34 tautological assertions and 15 no-oracle tests. Escalated to C9 and H10. |
| M9 | Agent instructions conflict | Confirmed, expanded | The two agent trees have also diverged from each other. Now M13. |
| Doc drift | `system.md`, `foc.md`, `foc-transforms.md`, `service-nvm.md` | Confirmed | Retained under Low. |
| Clean | `StrictMock` enforcement | Partially refuted | No forbidden mocks exist, but fixture-level `Times(AnyNumber())` neutralizes strictness on the calls under test. See H10. |
| Clean | No heap, formatting, build, tests | Confirmed | Re-run and re-validated. |

Eighteen findings are new in revision 2. The most consequential are C1, C3, C4, C8 and H3.

## Critical Findings

### C1. Calibration permanently unregisters the FOC control loop

`ThreePhaseInverter::PhaseCurrentsReady()` is a single-slot registration; every platform simply overwrites one member, for example [`PlatformFactoryImpl`](../../targets/platform_implementations/ti/implementation/PlatformFactoryImpl.cpp#L240-L242). [`Runner`](../../core/foc/instantiations/Runner.cpp#L5-L17) installs the FOC callback only in its constructor, and [`Runner::Enable()`](../../core/foc/instantiations/Runner.cpp#L24-L28) never re-installs it.

Every calibration service overwrites that same slot and leaves its own sampler or a no-op behind: [`MotorAlignmentImpl`](../../core/services/alignment/MotorAlignmentImpl.cpp#L48), [`ElectricalParametersIdentificationImpl`](../../core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.cpp#L94) and [`MechanicalParametersIdentificationImpl`](../../core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.cpp#L43).

After any successful calibration the registered callback is the alignment or identification lambda, not `Calculate()`. A subsequent enable starts the inverter while a stale calibration service consumes the samples and eventually calls `Stop()` on its own. The motor cannot be run until the device is rebooted.

**Required correction:** Make the registration scoped, with an explicit unregister or RAII token, or re-install the `Runner` callback immediately before `inverter.Start()`. Add a test that runs a full calibration and then asserts `Calculate()` is invoked on enable.

### C2. PWM can restart after disable, fault or emergency stop

[`Runner`](../../core/foc/instantiations/Runner.cpp#L9-L16) always calculates and writes duty cycles when the ADC callback runs, and no cascade `Calculate()` path reads the `enabled` flag. [`Runner::Disable()`](../../core/foc/instantiations/Runner.cpp#L29-L33) stops the inverter but neither gates nor unregisters the callback, and the ADC sequencer interrupt is not disabled.

On TI, `ThreePhasePwmOutput()` calls `Start()`, which re-enables both the output and the generator. A conversion already in flight when `Stop()` executed therefore re-energizes the bridge with a stale command, and the re-enabled generator re-triggers the ADC, so the loop self-sustains.

**Required correction:** Gate the write on the enabled state inside `Runner`, unregister the current-sample callback before stopping the inverter, and stop the ADC on the same path. Test the captured callback after `Disable()`, after emergency stop and after entering fault.

### C3. Unserialized EEPROM access resets the device

[`NonVolatileMemoryImpl`](../../core/services/non_volatile_memory/NonVolatileMemoryImpl.cpp#L56) guards busy-ness per logical record with four independent flags, but all four funnel into one EEPROM device whose driver asserts exclusivity.

Two consecutive operator commands are sufficient: an algorithm selection starts an asynchronous config write, and a following clear-calibration passes its own guard, reaches the EEPROM while the write is outstanding, and trips the assertion. The device hard-faults, potentially while the motor is spinning.

**Required correction:** Serialize at the `NonVolatileMemoryImpl` level with a single device-wide busy flag plus a bounded request queue, so no request reaches the EEPROM while another is outstanding.

### C4. Silently dropped NVM callbacks wedge the lifecycle

The busy guards at [`NonVolatileMemoryImpl`](../../core/services/non_volatile_memory/NonVolatileMemoryImpl.cpp#L191) discard the caller's callback without ever invoking it, while callers store a pending callback before issuing the request and rely on it firing.

A dropped calibration invalidation leaves the pending command callback set forever, so calibrate and clear-calibration are rejected for the rest of the power cycle. A dropped config save leaves the pending selection callback set, so every later mode selection answers busy. [`ControlModeStateMachine::Select()`](../../core/state_machine/ControlModeStateMachine.cpp#L148-L169) has also already mutated the default control mode before attempting the write, so the in-memory configuration now disagrees with both the active mode and the stored contents.

**Required correction:** Never drop a callback. Return an explicit busy status or queue the request, and mutate the configuration only after a successful write.

### C5. Hardware protection never reaches the production state machine

The target injects [`NoOpFaultNotifier`](../../targets/sync_foc_sensored/main/instantiations/Logic.hpp#L46), and [`PlatformFactory::RegisterBoardProtection()`](../../core/platform_abstraction/PlatformFactory.hpp#L75) has no caller outside tests. TI comparator events terminate at a fault lambda that is never installed; on ST and host the registration is an empty body.

The lifecycle can remain `Enabled` after a hardware protection event, contrary to REQ-SM-008, and nothing prevents the next duty-cycle write from re-enabling the outputs (C2).

**Required correction:** Add a platform-backed `FaultNotifier`, map every board-protection reason to a state-machine fault, and verify through a target or HIL test that a protection event stops the inverter and enters `Fault`.

### C6. Production position mode is unusable through both commissioning paths

The production target constructs its calibration services without a mechanical identification service, so with blank NVM [`PositionStateMachine`](../../core/state_machine/PositionStateMachine.cpp#L65-L75) enters `Fault` during calibration. `SpeedStateMachine` constructs its own estimator; `PositionStateMachine` does not.

With existing calibration, [`OuterLoopStateMachine::ApplyMechanics()`](../../core/state_machine/OuterLoopStateMachine.cpp#L18-L28) passes zero current and sampling placeholders. [`CascadeWithSpeedLoop::ConfigureMechanicsImpl()`](../../core/foc/cascade/CascadeWithSpeedLoop.cpp#L30-L36) repairs them into a local copy for the speed loop only, while [`PositionCascade::ConfigureMechanics()`](../../core/foc/cascade/PositionCascade.cpp#L24-L28) forwards the unrepaired struct to the position loop.

The consequence is exact, not approximate: the default position PID skips gain design on a zero sampling frequency and keeps zero tunings, so it outputs zero forever, and the LQR and LQI position laws are permanently rejected as having invalid parameters. Only the cascade-P law functions.

**Required correction:** Supply a production mechanical identification path and pass the effective maximum current and outer-loop sampling frequency to the position loop as well. Add a target-level test that calibrates position mode and observes a nonzero command for a nonzero position error.

### C7. Current PI integral gain omits the sample period

[`PidCurrentController::ApplyGains()`](../../core/foc/current_loop/PidCurrentController.cpp#L28-L39) supplies the continuous gain to an incremental PID. [`PidIncrementalBase`](../../infra/numerical-toolbox/numerical/controllers/implementations/PidIncremental.hpp#L110-L114) forms its first coefficient as the plain sum of the tunings and holds no sample-time member, so its integral argument must already be the per-sample product.

[`PidSpeedController::ApplyGains()`](../../core/foc/speed_loop/PidSpeedController.cpp#L35-L46) gets this right by multiplying with the outer sample period; the current controller does not. At the configured 10 kHz the integral action is about 10 000 times too large, the linear closed loop is grossly unstable, and only the output clamp keeps it bounded, degenerating the design into a bang-bang regulator.

The existing unit test asserts the defective relationship directly, so it must be corrected alongside the code.

**Required correction:** Multiply the integral gain by the sample period, and state the discrete convention explicitly in the current-loop theory document.

### C8. The control loop runs at 10 kHz while the documentation says 20 kHz

[`Logic.hpp`](../../targets/sync_foc_sensored/main/instantiations/Logic.hpp#L32) configures the inner loop at 10 000 Hz. About twenty statements assert 20 kHz, including [`system.md`](../architecture/system.md#L31), [`performance-optimization/README.md`](../performance-optimization/README.md#L45), `CLAUDE.md`, `.github/copilot-instructions.md`, `foc-cpp.instructions.md`, all four design documents, requirement REQ-FOC-005, and every agent file.

This is not confined to prose. [`cycle-analysis.json`](../../targets/sync_foc_sensored/main/cycle-analysis.json#L3) declares a 20 kHz loop rate and is consumed by the CI cycle-estimation job, so every per-period utilization figure CI reports is computed against the wrong period. The enforced 4500-cycle gate is 37.5 percent of the true 12 000-cycle period rather than the documented 75 percent, and the quoted 23 percent measured headroom is actually 11 percent.

The defect predates this branch, but the branch rewrote the affected documents and cycle-analysis configuration without reconciling them, which the documentation-first rule requires.

**Required correction:** Decide the real rate. Either raise the constant to 20 kHz and re-verify the gate, or set the loop rate to 10 kHz and correct every documentation, requirement and agent site.

### C9. Cascade tests assert tautologies

`hal::Percent` stores an unsigned 8-bit value, so all 34 occurrences of `EXPECT_GE(result.<phase>.Value(), 0)` across [`TestSpeedCascade.cpp`](../../core/foc/cascade/test/TestSpeedCascade.cpp#L72), `TestTorqueCascade.cpp` and `TestPositionCascade.cpp` are unsigned comparisons against zero and can never fail. The paired upper bound is satisfied unconditionally by the modulator's own clamp.

Fifteen tests contain no assertion other than that dead pair, including the ones named for wrap-seam handling, prefilter transparency and speed-loop bypass, which are precisely the properties a reviewer would expect to be covered. Each would pass with the named feature deleted.

**Required correction:** Give every named behavior a real oracle. The correct pattern already exists in `TestPositionControllerSelector` and `TestCurrentControllerSelector`, which compare against independently constructed references.

## High Findings

### H1. The low-priority callback has no unregister path

Speed and position cascades register lambdas capturing `this` in [`SpeedCascade`](../../core/foc/cascade/SpeedCascade.cpp#L9-L17) and [`PositionCascade`](../../core/foc/cascade/PositionCascade.cpp#L9-L16). The [`LowPriorityInterrupt`](../../core/foc/interfaces/Execution.hpp#L7-L13) contract has no unregister operation, and both implementations keep the stale handler alive.

If the inner loop has already triggered the handler when the mode is switched to torque, the queued event in [`LowPriorityInterruptImpl`](../../core/foc/instantiations/LowPriorityInterruptImpl.hpp#L12-L24) fires after the variant alternative has been destroyed and invokes the cascade on freed storage.

**Required correction:** Add an RAII registration token or explicit cancel, invoked from the cascade destructors, and drain any queued trigger before variant replacement.

### H2. Speed and position loops generate a first-sample speed impulse

[`EnableSpeedLoop()`](../../core/foc/cascade/CascadeWithSpeedLoop.cpp#L48-L59) zeroes both angle registers while the rotor sits at an arbitrary angle, so [`MeasureMechanicalSpeed()`](../../core/foc/cascade/CascadeWithSpeedLoop.cpp#L131-L137) interprets the whole first angle as motion. At 1 kHz a stationary rotor near pi radians appears as roughly 3140 rad/s, which rails the speed PI, destroys the ADRC disturbance estimate, and corrupts the mechanical estimator for hundreds of samples.

`TorqueCascade` already implements the correct pattern with a first-sample validity latch.

**Required correction:** Seed the differentiator from the first valid sample, as torque mode does. Test the first sample after enable at a nonzero stationary angle.

### H3. Online estimators are constructed at the inner rate but updated at the outer rate

[`SpeedStateMachine`](../../core/state_machine/SpeedStateMachine.cpp#L17-L18) constructs both online estimators with the base frequency, which in production is the 10 kHz PWM rate, while their update methods are invoked exclusively from the 1 kHz low-priority handler.

The mechanical estimator forms acceleration by multiplying the speed difference by the sampling frequency, so samples 1 ms apart are scaled by 10 000. The identified inertia comes out ten times too small, and the identified inductance ten times too small for the same reason. `ApplyOnlineEstimates` only rejects non-finite and non-positive values, so the mis-scaled estimates are applied to the live speed PI, the LQI plant and the ADRC input gain. The unit tests construct the estimators at the update rate, which is why CI does not see it.

**Required correction:** Pass the outer-loop frequency to both estimator constructors, and add a test that reflects the production wiring.

### H4. Electrical identification omits the wye correction and mis-signs the filter delay

The identification step drives one phase against the other two tied together, which measures 1.5 times the per-phase resistance and inductance. [`ElectricalParametersIdentificationImpl`](../../core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp#L29) declares exactly the constant needed for that correction, and nothing in the repository ever references it.

Independently, the moving-average group-delay correction subtracts the filter length where the buffering scheme requires adding half of it minus one, a seven-sample error in the wrong direction that under-estimates inductance by 40 to 70 percent in the realistic range. The theory document prescribes a third value that matches neither.

Both errors propagate into the current-loop proportional gain, the deadbeat plant inversion and the decoupling scale.

**Required correction:** Apply the wye correction, fix the delay compensation sign and magnitude, and reconcile the resistance and inductance estimation theory document with the excitation that is actually performed.

### H5. Flux linkage is hard-coded to zero, disabling back-EMF feedforward everywhere

[`FocStateMachineCommon`](../../core/state_machine/FocStateMachineCommon.cpp#L384-L394) is the only production configuration site and always passes zero flux linkage; [`CalibrationData`](../../core/services/non_volatile_memory/CalibrationData.hpp#L8-L28) has no field for it and alignment never estimates it.

Two consequences follow. [`CurrentControllerSelector`](../../core/foc/current_loop/CurrentControllerSelector.cpp#L8-L16) gates decoupled PID on a positive flux linkage, so that algorithm is permanently unselectable in production. More importantly, the back-EMF term is the dominant feedforward term at speed. For the simulator motor it exceeds the entire available voltage at 3000 rpm, while the cross-coupling term the code does apply is roughly a third of it, so deadbeat remains selectable while cancelling only the smaller term.

**Required correction:** Supply flux linkage from calibration or target configuration with a storage-layout migration, or remove the term and the gate and amend the theory document. Code and documentation must agree.

### H6. Persisted algorithms are applied before calibration loads, then discarded

[`ControlModeStateMachine::Activate()`](../../core/state_machine/ControlModeStateMachine.cpp#L293-L326) applies persisted identifiers immediately after constructing a mode, before the motor model exists. Calibration and motor parameters arrive asynchronously afterwards through [`FocStateMachineCommon`](../../core/state_machine/FocStateMachineCommon.cpp#L314-L338).

Any algorithm whose selectability depends on identified parameters is therefore rejected during boot, and the fallback is written back over the user's stored choice, so the preference is destroyed rather than merely ignored. Deadbeat, decoupled PID, speed LQI and ADRC, and position LQR and LQI are all affected. This violates REQ-CTRL-006.

**Required correction:** Apply persisted algorithms only after calibration reaches `Ready`, and distinguish temporarily unavailable parameters from invalid persisted bytes; only the latter may overwrite the stored configuration.

### H7. No production CLI path can command a setpoint or a bandwidth

The per-mode terminal interactors are constructed only in their own unit tests. The production target builds only [`RegisterCliCommands()`](../../core/state_machine/ControlModeStateMachine.cpp#L473-L535), which registers lifecycle and algorithm commands but no setpoint or bandwidth command. The mode-specific registration is additionally gated on the CLI transition policy while `Activate()` hard-codes the automatic policy, so the estimator-status command is unreachable too.

The design document also disagrees on spelling and arity: [`service-cli.md`](../design/service-cli.md#L50-L115) documents a hyphenated command taking two axis currents, while [`TerminalTorque.cpp`](../../core/services/cli/TerminalTorque.cpp#L10-L35) registers an underscored command taking one value.

**Required correction:** Register mode-aware setpoint and bandwidth wrappers on the coordinator, which already owns lifetime-safe access to the active controller, then correct the CLI design document.

### H8. Sliding-mode control cannot hold a nonzero current reference

[`SlidingModeCurrentController`](../../core/foc/current_loop/SlidingModeCurrentController.hpp#L28-L36) calls the reference overload of [`SlidingModeControl`](../../infra/numerical-toolbox/numerical/robust_control/SlidingModeControl.hpp#L81-L89), which computes control purely from the state error and therefore commands zero voltage at the reference. The equivalent-control law is missing the equilibrium input, which for this plant is exactly the resistive drop at the reference current.

The residual is quantifiable: the steady-state error ratio is the discrete decay deficit divided by one plus the switching-to-boundary ratio, which is about minus 1.9 percent at the default gains and grows toward minus 11 percent as that ratio falls. With no integral action nothing removes it.

**Required correction:** Add the equilibrium feedforward term and update the matching theory equation. Add a closed-loop nonzero-current hold test with resistive voltage drop.

### H9. CAN input is unvalidated and acknowledgements do not reflect behavior

[`FocMotorCanBridge::OnStart()`](../../core/state_machine/FocMotorCanBridge.cpp#L18-L21) acknowledges success even when the enable command is rejected; stop, clear-fault and emergency-stop behave the same way. [`OnIdentifyElectrical()`](../../core/state_machine/FocMotorCanBridge.cpp#L48-L56) discards the parameter-result callback, so identified values never reach the bus.

Separately, decoded setpoints are accepted across the full signed 16-bit range without clamping, which permits a torque request of over 320 A against a 15 A hardware limit, and are accepted in any lifecycle state including `Fault`, with the stored value re-applied at the next enable. An unrecognized control-mode byte is silently mapped to torque, which both accepts invalid input and makes the invalid-mode result unreachable.

**Required correction:** Return command results to the bridge and map them to acknowledgement or error. Return identified parameters through the protocol callback. Clamp every decoded setpoint to the configured limits, reject out-of-range payloads explicitly, and reject setpoints outside the running states.

### H10. Requirement-tagged and mock-based tests do not discriminate

Three requirement-tagged SIL scenarios verify nothing. [`SpeedFunctionalSteps.cpp`](../../integration_tests/software_in_the_loop/steps/SpeedFunctionalSteps.cpp#L43-L75) and [`PositionFunctionalSteps.cpp`](../../integration_tests/software_in_the_loop/steps/PositionFunctionalSteps.cpp#L43-L96) assert only success, contain no assertion at all, or assert a property unrelated to the requirement that holds even if the method under test is a no-op. The comments in these steps claim verification that does not occur, and the feature text still describes per-axis PID gains that this branch removed. Requirement traceability pointing at an unconditional success is worse than no traceability.

`TestRunner` matches the PWM output with a wildcard while constructing an unused expected value, and its fixture applies an any-number expectation to `Stop()` and `Disable()`, which is the exact shutdown ordering `Runner` exists to guarantee. The SIL fixtures do the same to the low-priority registration, permitting zero registrations. Forbidden mock types are genuinely absent, but the effect on these calls is the same.

The SVM continuity test in [`TestSpaceVectorModulation.cpp`](../../core/foc/transforms/test/TestSpaceVectorModulation.cpp#L107-L165) calls the generator twice with identical input and compares the results, which is guaranteed for a pure function, and the all-sector assertions sort the duties and erase phase identity. Active-algorithm CLI tests in [`TestControlModeStateMachine.cpp`](../../core/state_machine/test/TestControlModeStateMachine.cpp#L1007-L1053) invoke the command but never inspect the output.

**Required correction:** Give each tagged scenario a real assertion or remove the tag. Match the expected duty cycles. Remove the fixture-level wildcards on the behavior under test. Assert phase-specific SVM outputs on both sides of each sector boundary and the emitted CLI algorithm names.

### H11. Requirements validation fails

[`controller-selection.yaml`](../requirements/foc/controller-selection.yaml#L115-L128) adds a `status` property to two requirements. [`requirement.schema.json`](../tools/requirement.schema.json#L11-L28) permits only `id`, `title` and `shall` because additional properties are forbidden. A re-run of the schema against all eleven requirement files and 74 requirements produced exactly these two violations, so the requirements-validation CI job fails on this branch.

**Required correction:** Add a constrained `status` property to the schema and downstream tooling, or remove the unsupported fields.

### H12. Normative requirements still describe the removed fixed-PID architecture

The old contracts remain in [`torque-controller.yaml`](../requirements/foc/torque-controller.yaml#L1-L6), which mandates separate PID controllers, [`speed-controller.yaml`](../requirements/foc/speed-controller.yaml#L14-L25), which mandates independent axis and raw outer PID gains, and [`position-controller.yaml`](../requirements/foc/position-controller.yaml#L1-L26), which mandates an unconditional triple-PID cascade.

All three conflict with selectable algorithms, bandwidth-based tuning, and position laws that bypass the speed loop entirely.

**Required correction:** Rewrite these requirements around selectable laws, loop-specific bandwidth contracts, output-kind routing, and required anti-windup behavior.

## Medium Findings

### M1. Algorithm selection mutates a variant the ISR visits

[`ControllerSelector`](../../core/foc/selection/ControllerSelector.hpp#L30-L40) replaces the active alternative from CLI or CAN context, while its dispatch visits the same variant from the inner-loop interrupt. There is no critical section, no atomic publication, and, because of C2, no reliable enabled gate. A conversion completing mid-replacement dispatches on a partially constructed alternative.

**Required correction:** Mask the control interrupt around replacement, or double-buffer and publish the new alternative through a single atomic index, as the estimator channel already does correctly.

### M2. Mode switching can destroy a state machine with in-flight asynchronous work

[`ControlModeStateMachine::Select()`](../../core/state_machine/ControlModeStateMachine.cpp#L148-L169) checks only that the active machine is stopped, not that it has a pending command. [`Activate()`](../../core/state_machine/ControlModeStateMachine.cpp#L293-L326) then replaces the alternative while boot and calibration callbacks capturing `this` may still be outstanding. The in-tree state guards inside those callbacks run after the object is destroyed and therefore read freed memory.

This is rated medium rather than critical because the TI EEPROM read path completes synchronously today, which closes the boot-load window; the erase and write paths remain exposed.

**Required correction:** Reject mode replacement while any operation owned by the active machine is pending, or introduce cancellable callback ownership. Test deferred boot-load and deferred calibration-clear under ASan.

### M3. Emergency stop discards a valid calibration

Emergency stop drops a calibrated machine to `Idle` even though the calibration data remains valid in memory and in NVM, forcing a full recalibration afterwards. The transition is not documented in [`state-machine.md`](../design/state-machine.md#L53-L59), and the command itself is absent from the entire documentation tree.

**Required correction:** Transition to `Ready` when calibration data is valid, and document the command and its transitions.

### M4. Services stop the inverter outside the lifecycle owner

The alignment and identification services call `Stop()` on the driver directly from their own handlers, for example in [`MotorAlignmentImpl`](../../core/services/alignment/MotorAlignmentImpl.cpp#L76-L88). Combined with C1 those handlers stay installed, so the state machine reports `Enabled` while the bridge is off, and their sample counters resume from whatever value the previous run left behind.

**Required correction:** Scope the registrations, and route all inverter stop requests through the lifecycle owner.

### M5. Two-DOF position filtering takes the long path across angle wrap

[`TwoDofPositionController`](../../core/foc/position_loop/TwoDofPositionController.hpp#L15-L25) applies an ordinary exponential moving average to raw single-turn angles. An angle is not a linear quantity, so a step across the seam interpolates the long way and a 16-degree move is executed as a 344-degree rotation. The filter additionally resets to zero rather than the current angle, so every enable ramps the reference from absolute zero.

**Required correction:** Filter wrapped angular deltas or maintain an unwrapped continuous reference, and seed the filter state from the measured angle on reset. Test both seam-crossing directions.

### M6. Decoupled PI limits voltage before and after feedforward

[`PidCurrentController`](../../core/foc/current_loop/PidCurrentController.hpp#L18-L25) applies the circular limit before [`DecoupledPidCurrentController`](../../core/foc/current_loop/DecoupledPidCurrentController.hpp#L12-L20) adds feedforward and limits again.

This wastes available voltage when feedforward partially cancels a saturated demand, and the integrator winds up by the difference between what it believes it delivered and what the projection actually applied. The current-loop theory document assumes the clamped quantity is the applied output, which is false once feedforward is inserted between the clamp and the limiter.

**Required correction:** Combine PI and feedforward before one shared vector limit, with saturation feedback based on the final applied vector.

### M7. Speed LQI anti-windup is one sample late

[`LqiSpeedController::Compute()`](../../core/foc/speed_loop/LqiSpeedController.cpp#L35-L47) decides whether to integrate from the previous halted state and detects current saturation only after the current sample has integrated, permitting one saturated integration step at each onset.

The comment and the theory document both describe same-sample conditional integration.

**Required correction:** Roll back the just-added integral state on current saturation or determine saturation before committing it. Align the comment and theory with the corrected behavior.

### M8. The inner-loop path still uses virtual dispatch

[`Runner`](../../core/foc/instantiations/Runner.cpp#L9-L16) invokes `Calculate()` through a base reference. This contradicts the explicit hot-path rule and REQ-CTRL-005, and the usual mitigation that measurements remain below budget cannot currently be relied upon because the budget percentages themselves are wrong (C8).

**Required correction:** Template `Runner` on the concrete cascade or register the callback from `FocController`, then verify generated assembly and rerun cycle-budget analysis against the corrected loop rate.

### M9. `Runner.cpp` lacks the mandatory hot-path optimization guard

[`Runner.cpp`](../../core/foc/instantiations/Runner.cpp#L1-L4) is the interrupt entry point for the entire inner loop and carries neither the guarded optimization pragma nor the speed attribute. It is the only hot-path implementation file under `core/foc/` without it, against 21 files that have it. Conversely, `TorqueCascade` applies the speed attribute to `Enable()`, `Disable()` and `SetPoint()`, which run at command rate.

**Required correction:** Add the guard to `Runner.cpp` and remove it from the command-rate methods.

### M10. New control-loop code is substantially untested

`core/foc/selection/` has no test directory at all, despite the selector template underpinning all three loops. In `core/foc/position_loop/`, only the selector has a test file: the PID, two-DOF, LQR, LQI and cascade-P position controllers and the position plant model have no direct coverage. The current and speed loops each have five test files, so the position loop, which has the most algorithm variants, is the outlier. The current and speed plant models and the angle-wrap helper are also untested.

**Required correction:** Add per-controller tests mirroring the current-loop suites, and a test directory for the selector template.

### M11. NVM load cannot fail, and invalidation is unverified

Config load substitutes defaults and reports success for a bad magic, a version mismatch and a CRC mismatch, so corruption is indistinguishable from never-provisioned and the target's fallback branch is unreachable. Calibration invalidation reports success straight from the erase completion with no read-back, unlike the save path which verifies. [`service-nvm.md`](../design/service-nvm.md#L76-L77) documents the opposite behavior for both.

**Required correction:** Return distinct statuses and let the caller decide, verify the erase by read-back, and reconcile the design document.

### M12. The accepted selector design mixes implemented and deferred behavior

[`controller-selection.md`](../design/controller-selection.md#L91-L93) says ILC is deliberately absent, but later sections specify ILC selection, tuning, and persistence. The same document claims CAN algorithm selection and persistent friction compensation that the current enums, CAN bridge, and NVM records cannot represent.

**Required correction:** Move future behavior into an explicitly deferred design or remove it from the accepted current-state design. Document only fields and commands implemented by `ConfigData`, `CalibrationData`, CLI, and CAN.

### M13. Agent and instruction files conflict with the repository and with each other

[`reviewer.agent.md`](../../.github/agents/reviewer.agent.md#L1-L5) grants only read and search tools despite requiring changed-file discovery and build verification. Its checklist also presents the balanced-current Clarke reduction as the general transform, references a `Rpm` alias that does not exist (the real one is `RevPerMinute`), references a `Driver` interface this branch deleted, and forbids plain `TEST()` despite the testing instructions allowing stateless tests.

The two agent trees have also diverged: the `.github/` planner and executor prescribe a `host-Debug` test preset that does not exist, while their `.claude/` counterparts correctly use `host`. The real test presets are `defaults`, `host`, `coverage` and `host-single-Debug`.

**Required correction:** Synchronize both agent trees with `.github/copilot-instructions.md`, `foc-cpp.instructions.md`, `testing.instructions.md`, and `CMakePresets.json`. Give the reviewer read-only command, build and test capabilities.

## Low Findings

- `E_FOC_AUTO_TRANSITION_POLICY` is a dead build option: it is declared, compiled in and set in the presets, but no source reads the macro. [`state-machine.md`](../design/state-machine.md#L180-L187) still describes the build-time mechanism as normative.
- `LastFaultCode()` initializes to a hardware-fault code, so clients cannot distinguish "no fault yet" from a real hardware fault.
- The mechanical estimator never resets its previous-speed register on enable, so the H2 impulse is fed into the recursive least squares and persists for hundreds of samples.
- `WrappedPositionError` performs a single correction, so a setpoint beyond three pi radians rotates the wrong way, and the setpoint is stored unconstrained.
- The speed and position plant models use a first-order Euler decay with no guard, which can go non-physical and be fed into the Riccati solve, and is reachable given the H3 inertia error.
- The runner installs the current-sample callback before any configuration, so a conversion in that window runs with zero gains.
- Duty cycles are quantized to whole percent, imposing a current-resolution floor that the theory document contradicts with a claimed 16-bit resolution. This may predate the branch.
- Region erase always writes the full region rather than the record, tripling EEPROM wear per config persist.
- Dead declarations and unused includes left by the move: an unreferenced tolerance constant and unused includes in the cascade tests, an unused integer header in the selector, and unused using-declarations in the runner test.
- The transform headers use snake_case locals against the camelCase rule, duplicate the same constants at two different precisions, and retain redundant functional casts and local aliases.
- Test naming style is split between PascalCase and snake_case across the new suites, and several fixtures use assignment initialization where the rule prefers braces.

## Documentation Drift

- [`system.md`](../architecture/system.md#L108-L120) retains the deleted implementations sub-layer in the FOC core table and still assigns driver ports to the FOC interfaces layer.
- [`foc-transforms.md`](../design/foc-transforms.md#L55-L66) says both that all three currents are measured and that the third is derived; its interface section describes two-input Clarke and sector-based SVM rather than the implemented three-input Clarke and common-mode injection.
- [`service-nvm.md`](../design/service-nvm.md#L76-L77) promises invalid-data and version statuses for config load, while the implementation returns the default config with success.
- [`foc.md`](../theory/foc.md#L319) states that 3000 rpm with five pole pairs advances 0.09 degrees per sample; the correct value is 4.5 degrees, which also invalidates the sentence's conclusion. The same document contains a worked example whose inequality is self-contradictory and which feeds physical volts into a per-unit modulator, and its numerical-properties table states a per-axis voltage limit where the current-loop document states a circular one.
- The speed-loop theory document's justification for unit-sample integration is mathematically incorrect, and the position-loop document specifies a different state-space formulation and a different LQI construction than the implementation uses. The implementation's algebra was verified self-consistent, so the documents are what must change.
- [`orchestrator.agent.md`](../../.github/agents/orchestrator.agent.md#L55-L59) links to a numerical-toolbox path that predates the `infra/` layout and does not resolve. It is the only broken relative link in the documentation set.
- Three requirement identifiers referenced by SIL feature files are defined nowhere, 37 of 74 requirements are referenced nowhere outside the requirements directory, and two identifiers collide with the `can-lite` submodule's own corpus, producing a false trace.

## Verified Clean Categories

Checked and found correct, with no action required:

- Clarke and Park signs, amplitude-invariant scaling, three-phase form, and electrical-angle multiplication by pole pairs.
- q-axis torque mapping.
- Space Vector Modulation common-mode injection, output bounds, scaling self-consistency end to end, and continuity by construction. The circular voltage limiter is correct and applied in all four current controllers.
- Fast trigonometry lookup table values, branch-free wrap for negative indices, and interpolation error bound.
- Deadbeat gain derivation for one-step and two-step horizons, including exact zero-order-hold discretization and exact DC tracking.
- ADRC observer gains, input gain, unit consistency, bandwidth clamp, and its exemplary anti-windup, which feeds the clipped command back to the observer.
- Speed PI pole-zero cancellation and position PID envelope normalization, both with the correct discrete integral convention.
- LQI speed augmentation structure, and the position LQR and LQI time-scaled formulation.
- Cascade bandwidth separation ratios.
- Strong unit typing throughout, with correct inductance, rpm and mechanical-versus-electrical conversions.
- The estimator double-buffer hand-off from interrupt to low-priority context.
- Fixed-size variant storage for controller algorithms, with no allocation.
- Persisted enum range validation before casting, and NVM record magic, version and CRC integrity with static assertions on layout.
- Asynchronous callback state guards, which correctly handle state change though not object destruction.
- No heap allocation or dynamic STL usage in changed embedded code, no pure virtual destructors, no recursion, and no blocking calls in interrupt-reachable paths.
- No forbidden mock types anywhere in source, no disabled or skipped tests, and no leftover work markers in `core/`.
- No leftover files from the deleted implementations layer, every new source and test file registered with a CMake target, and all fixtures correctly placed in anonymous namespaces with the macros outside.
- Changed-file C++ formatting, patch whitespace, and changed YAML and JSON syntax.

## Validation Results

| Check | Result |
|---|---|
| Host Debug build | Passed, no errors or warnings |
| Host CTest suite | 23/23 passed |
| EK-TM4C1294XL Debug build | Passed, no errors or warnings |
| Changed-file `clang-format --dry-run --Werror` | Passed |
| `git diff --check` | Passed |
| Requirement schema validation, 11 files and 74 requirements | Failed: two violations |
| Markdown link and line-anchor integrity | Failed: one broken link; all line anchors valid |
| Requirement traceability | 3 dangling identifiers, 37 untraced, 2 colliding |
| Production no-heap scan | Passed |
| Forbidden mock scan | Passed, with the strictness caveat in H10 |

## Final Assessment

The branch builds cleanly on host and target and its tests pass, but it is not ready to merge. The layering refactor itself is sound: the transform, deadbeat, ADRC and gain-design mathematics verified clean, and the new directory structure is well separated. It sits, however, on top of defects that prevent the product from working and, in three cases, from recovering without a reboot.

The release blockers are the FOC control loop being permanently unregistered by calibration, PWM re-energizing after stop, unserialized EEPROM access resetting the device, silently dropped persistence callbacks wedging the lifecycle, unwired hardware protection, an unusable production position mode, a current PI that is four orders of magnitude over-gained, a documented loop rate that is twice the real one including in a CI-consumed configuration file, and a cascade test suite whose assertions cannot fail.
