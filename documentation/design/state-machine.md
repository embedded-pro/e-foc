---
title: "Service: FOC State Machine"
type: design
status: draft
version: 0.1.0
component: state-machine
date: 2026-04-10
---

| Field     | Value                      |
|-----------|----------------------------|
| Title     | Service: FOC State Machine |
| Type      | design                     |
| Status    | draft                      |
| Version   | 0.1.0                      |
| Component | state-machine              |
| Date      | 2026-04-10                 |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
> Prose descriptions of algorithms are encouraged; source-level details are not.
>
> **Diagrams**: All visuals must be either a Mermaid fenced code block (` ```mermaid `) or ASCII art inline
> in the document. External image references (`![alt](path)`) are **not allowed**.

---

## Responsibilities

**Is responsible for:**
- Owning the complete motor lifecycle: `Idle → Calibrating → Ready ⇄ Enabled`, and `Fault` as an escape state reachable from any active state
- Enforcing all transition guards so that the FOC controller can only be enabled after a successful calibration, and calibration can only be started from `Idle` or `Ready`
- Orchestrating the sequential calibration chain: pole-pair identification → resistance and inductance estimation → alignment → (mechanical parameter identification for speed/position modes) → NVM persistence
- On successful boot with valid NVM data, automatically loading calibration and transitioning to `Ready` without requiring user action
- Registering lifecycle commands on the terminal in CLI-driven mode (`calibrate`, `enable`, `disable`, `clear_fault`, `clear_cal`)
- Intercepting hardware fault notifications from the fault notifier and immediately transitioning to `Fault`, stopping the inverter if it was active

**Is NOT responsible for:**
- Executing FOC calculations — those remain the responsibility of the control-mode implementation (Torque, Speed, Position)
- Directly driving hardware peripherals — all hardware interaction is delegated to the calibration services and the FOC controller
- Storing persistent calibration data itself — that is the responsibility of the Non-Volatile Memory service
- Implementing the identification algorithms — those are encapsulated in the Electrical and Mechanical Parameters Identification services

---

## Component Details

### Motor Lifecycle States

The state machine has five named states:

| State         | Motor condition                                                              | Allowed transitions                                                                                                         |
|---------------|------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `Idle`        | No calibration data; motor cannot be enabled                                 | → `Calibrating` (CmdCalibrate), → `Ready` (valid NVM on boot), → `Fault` (hardware fault)                                   |
| `Calibrating` | Calibration sequence in progress; motor is driven by identification services | → `Ready` (sequence complete + NVM saved), → `Fault` (any step fails or hardware fault)                                     |
| `Ready`       | Calibration data valid and applied; motor can be enabled at any time         | → `Enabled` (CmdEnable), → `Calibrating` (CmdCalibrate re-runs), → `Idle` (CmdClearCalibration), → `Fault` (hardware fault) |
| `Enabled`     | FOC controller active; motor under closed-loop control                       | → `Ready` (CmdDisable), → `Fault` (hardware fault)                                                                          |
| `Fault`       | Safe state; inverter stopped; fault code recorded                            | → `Idle` (CmdClearFault)                                                                                                    |

### State Diagram

```mermaid
stateDiagram-v2
    [*] --> Idle

    Idle --> Calibrating : CmdCalibrate
    Idle --> Ready : valid NVM on boot
    Idle --> Fault : hardware fault

    Calibrating --> Ready : sequence complete\n+ NVM saved
    Calibrating --> Fault : any step fails\nor hardware fault

    Ready --> Enabled : CmdEnable
    Ready --> Calibrating : CmdCalibrate\n(re-calibrate)
    Ready --> Idle : CmdClearCalibration
    Ready --> Fault : hardware fault

    Enabled --> Ready : CmdDisable
    Enabled --> Fault : hardware fault

    Fault --> Idle : CmdClearFault
```

### Calibration Sequence

When `CmdCalibrate` is issued from `Idle` or `Ready`, the state machine enters `Calibrating` and executes a sequential chain of identification steps. Each step is asynchronous: the state machine calls a service and awaits a callback before proceeding to the next step. If any step returns a failure result, the machine immediately enters `Fault`.

```mermaid
sequenceDiagram
    participant SM as FOC State Machine
    participant EI as Electrical Ident
    participant MA as Motor Alignment
    participant MI as Mech Ident (speed modes)
    participant NVM as Non-Volatile Memory

    SM->>EI: EstimateNumberOfPolePairs
    EI-->>SM: result (polePairs or failure)
    SM->>EI: EstimateResistanceAndInductance
    EI-->>SM: result (R, L or failure)
    SM->>MA: ForceAlignment(polePairs)
    MA-->>SM: result (encoderOffset or failure)
    note over SM,MI: Speed and position modes only
    SM->>MI: EstimateFrictionAndInertia
    MI-->>SM: result (B, J or failure)
    SM->>NVM: SaveCalibration(data)
    NVM-->>SM: status
    note over SM: On success: apply data, enter Ready
    note over SM: On any failure: enter Fault
```

**Steps and data produced:**

| Step                                           | Service             | Data stored                                              |
|------------------------------------------------|---------------------|----------------------------------------------------------|
| 1. Pole pairs                                  | Electrical Ident    | `polePairs`                                              |
| 2. Resistance and inductance                   | Electrical Ident    | `rPhase`, `lD`, `lQ`                                     |
| 3. Alignment                                   | Motor Alignment     | `encoderZeroOffset`                                      |
| 4. Mechanical parameters (speed/position only) | Mechanical Ident    | `inertia`, `frictionViscous`, `kpVelocity`, `kiVelocity` |
| 5. NVM persist                                 | Non-Volatile Memory | All of the above written to EEPROM                       |

After saving, calibration data is applied to the FOC controller (current PID gains computed from R/L/bandwidth, encoder zero offset applied, velocity PID gains applied for speed modes), and the state machine transitions to `Ready`.

### Fault Safety

Entering `Fault` always stops the inverter when the machine was in `Enabled` or `Calibrating` state. This ensures that any active PWM output (from normal operation or from identification test signals) is immediately cut, regardless of which state caused the fault.

The last fault code is preserved in `LastFaultCode()` and remains readable even after the fault is cleared via `CmdClearFault`.

### Transition Policies

The state machine supports two transition policies, selected at instantiation time:

- **CLI policy (default)**: The state machine registers the commands `calibrate`, `enable`, `disable`, `clear_fault`, and `clear_cal` on the connected terminal. Users interact via a serial console.
- **Automatic policy**: No terminal commands are registered. The caller drives transitions programmatically by invoking `CmdCalibrate()`, `CmdEnable()`, `CmdDisable()`, `CmdClearFault()`, and `CmdClearCalibration()` directly — for example, from CAN message handlers or automated test sequences.

### Boot-Time NVM Check

On construction, the state machine asynchronously checks whether valid calibration data exists in NVM. If data is found and loads successfully, calibration is applied and the machine transitions directly to `Ready` — no user action required. If the check fails or the data is absent, the machine starts in `Idle`.

---

## Interfaces

### Provided

| Interface               | Purpose                                                          | Contract                                                                                                                         |
|-------------------------|------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------|
| `FocStateMachineBase`   | Abstract lifecycle controller — state query and command dispatch | Constructed once per application; all command methods are safe to call from any state (invalid transitions are silently ignored) |
| `CurrentState()`        | Returns the current `State` variant for inspection               | Returns a const reference; valid for the lifetime of the state machine                                                           |
| `LastFaultCode()`       | Returns the most recent fault code                               | Value is only meaningful when in `Fault` state or just after clearing a fault                                                    |
| `CmdCalibrate()`        | Requests start of calibration                                    | Only effective from `Idle` or `Ready`; ignored from all other states                                                             |
| `CmdEnable()`           | Requests enabling the FOC controller                             | Only effective from `Ready`; ignored from all other states                                                                       |
| `CmdDisable()`          | Requests disabling the FOC controller                            | Only effective from `Enabled`; ignored from all other states                                                                     |
| `CmdClearFault()`       | Clears the fault and returns to `Idle`                           | Only effective from `Fault`; ignored from all other states                                                                       |
| `CmdClearCalibration()` | Invalidates NVM calibration and returns to `Idle`                | Ignored when in `Enabled`; effective from all other states                                                                       |

### Required

| Interface                            | Purpose                                                                  | Contract                                                                       |
|--------------------------------------|--------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| `NonVolatileMemory`                  | Persists and retrieves calibration data across power cycles              | Must remain valid for the lifetime of the state machine                        |
| `ElectricalParametersIdentification` | Estimates pole pairs, phase resistance, and dq inductances               | Operations are asynchronous; callback fires on the same event loop             |
| `MotorAlignment`                     | Forces rotor to a known angle and returns the encoder zero offset        | Operation is asynchronous; result is optional (nullopt = failure)              |
| `MechanicalParametersIdentification` | Estimates rotor inertia and viscous friction (speed/position modes only) | Operation is asynchronous; result is optional (nullopt = failure)              |
| `FaultNotifier`                      | Delivers hardware fault notifications to the state machine               | `Register()` must be called during construction; callback may fire at any time |
| `ThreePhaseInverter`                 | Used by the FOC controller to issue PWM and read phase currents          | Stopped immediately on any fault from `Enabled` or `Calibrating` state         |
| `Encoder`                            | Rotor position sensor; zero offset applied after alignment               | `Set()` called during `ApplyCalibrationData` to configure the zero point       |
| `TerminalWithStorage`                | Serial command interface for CLI-mode transition policy                  | Commands registered in constructor; terminal must outlive the state machine    |
| `Tracer`                             | Debug trace output for lifecycle events                                  | All state transitions and calibration steps are traced                         |

---

## Data Model

| Entity            | Field                     | Type / Unit                    | Range    | Notes                                                                                                                   |
|-------------------|---------------------------|--------------------------------|----------|-------------------------------------------------------------------------------------------------------------------------|
| `CalibrationData` | `polePairs`               | count (uint8)                  | 1–255    | Number of electrical pole pairs                                                                                         |
| `CalibrationData` | `rPhase`                  | Ohm (float)                    | > 0      | Phase resistance identified by electrical ident                                                                         |
| `CalibrationData` | `lD` / `lQ`               | mH (float)                     | > 0      | D/Q inductances (set equal; anisotropy not estimated)                                                                   |
| `CalibrationData` | `encoderZeroOffset`       | int32 (bit-cast float Radians) | any      | Quantised electrical angle at encoder zero; applied via `Encoder::Set()`                                                |
| `CalibrationData` | `inertia`                 | N·m·s² (float)                 | ≥ 0      | Rotor inertia; populated only for speed/position modes                                                                  |
| `CalibrationData` | `frictionViscous`         | N·m·s/rad (float)              | ≥ 0      | Viscous friction coefficient; populated only for speed/position modes                                                   |
| `CalibrationData` | `frictionCoulomb`         | N·m (float)                    | ≥ 0      | Coulomb friction; currently 0 (not identified)                                                                          |
| `CalibrationData` | `kpVelocity`              | (float)                        | ≥ 0      | Velocity PID proportional gain; computed as J × ω_bw                                                                    |
| `CalibrationData` | `kiVelocity`              | (float)                        | ≥ 0      | Velocity PID integral gain; computed as B × ω_bw                                                                        |
| `CalibrationData` | `kpCurrent` / `kiCurrent` | (float)                        | any      | Current PID gains; computed from R/L/bandwidth by auto-tuner, not stored by the state machine                           |
| `FaultCode`       | —                         | enum (uint8)                   | 7 values | `overcurrent`, `overvoltage`, `overtemperature`, `encoderLoss`, `watchdogTimeout`, `hardwareFault`, `calibrationFailed` |

---

## Error Handling

All calibration steps produce `std::optional` results. A `nullopt` from any step immediately enters `Fault` with code `calibrationFailed`. The entire calibration sequence is safe to retry by issuing `CmdCalibrate` after `CmdClearFault`.

NVM operations (save, load, invalidate) are also asynchronous and report a `NvmStatus`. On any non-`Ok` status during save or load, the machine enters `Fault` or remains in `Idle`, respectively, without corrupting application state.
