---
title: "Service: Mechanical Parameters Identification"
type: design
status: draft
version: 0.1.0
component: service-mechanical-ident
date: 2026-04-07
---

| Field     | Value                                         |
|-----------|-----------------------------------------------|
| Title     | Service: Mechanical Parameters Identification |
| Type      | design                                        |
| Status    | draft                                         |
| Version   | 0.1.0                                         |
| Component | service-mechanical-ident                      |
| Date      | 2026-04-07                                    |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
> Prose descriptions of algorithms are encouraged; source-level details are not.
>
> **Diagrams**: All visuals must be either a Mermaid fenced code block (` ```mermaid `) or ASCII art inline
> in the document. External image references using Markdown image syntax are **not allowed**.

---

## Responsibilities

**Is responsible for:**
- Estimating rotor moment of inertia (J) and viscous friction coefficient (B) while the motor runs in closed-loop speed control
- Computing instantaneous electromagnetic torque from the q-axis current and the caller-supplied torque constant
- Estimating angular acceleration by finite-differencing successive speed measurements obtained from the encoder
- Maintaining a real-time Recursive Least Squares (RLS) estimator that continuously refines the parameter vector [J, B, τ_friction] using each new observation
- Enforcing a configurable one-shot estimation timeout, after which the latest RLS estimates are reported and the procedure stops
- Delivering results through a single completion notification that reports friction and inertia using explicit physical units

**Is NOT responsible for:**
- Starting or configuring the speed control loop — an external speed-control function must already be active when the procedure begins
- Persisting the returned parameters — the caller decides what to do with them
- Auto-tuning the speed controller — this service provides the plant parameters that a separate tuning step may consume
- Measuring electrical parameters (R, L, pole pairs) — those are handled by the Electrical Parameters Identification service

---

## Component Details

### Prerequisites and Motor Model

The mechanical dynamics of a PMSM rotor are governed by Newton's second law for rotation:

```
τ_motor = J · dω/dt + B · ω + τ_friction
```

where:

- **τ_motor** — electromagnetic torque produced by the motor (N·m)
- **J** — rotor moment of inertia (kg·m²)
- **B** — viscous friction coefficient (N·m·s/rad)
- **τ_friction** — Coulomb (constant) friction term (N·m)
- **ω** — rotor angular velocity (rad/s)
- **dω/dt** — rotor angular acceleration (rad/s²)

This is a linear model in the three unknowns [J, B, τ_friction]. The RLS estimator treats each measurement instant as one observation of this linear equation.

### Torque Estimation

At each sampling callback, the instantaneous electromagnetic torque is estimated from the q-axis current:

```
τ_motor = Kt × Iq
```

where `Kt` is the torque constant (N·m/A) provided by the caller. The q-axis current `Iq` is the measured value supplied by the `ThreePhaseInverter` ADC callback — the same current feedback path used by the FOC current controller.

The d-axis contribution to torque is zero for surface PMSM under the Id = 0 operating condition assumed during identification. For interior PMSM with reluctance torque, this approximation introduces a small systematic error in the torque estimate; the caller should account for this if Ld ≠ Lq.

### Speed and Acceleration Estimation

Angular velocity (ω) at each observation instant is obtained from two successive encoder angle samples using a finite difference with wrap-around compensation:

```
ω_k = (θ_k − θ_{k−1}) / Δt
```

where Δt is the sampling period and wrap-around compensation shifts the raw difference into the range (−π/Δt, +π/Δt) rad/s in exactly the same manner used by the speed control outer loop.

Angular acceleration (dω/dt) is obtained from two successive velocity estimates by a further finite difference:

```
α_k = (ω_k − ω_{k−1}) / Δt
```

The double finite difference amplifies noise; the quality of the acceleration estimate therefore depends on the encoder resolution and sampling rate. Low-resolution encoders or very low speeds produce noisy acceleration estimates and degrade identification accuracy. The caller is advised to command a non-zero target speed of sufficient magnitude to obtain a good signal-to-noise ratio.

### Recursive Least Squares Estimator

The RLS estimator maintains a 3×1 parameter vector θ = [J, B, τ_friction]ᵀ and a 3×3 covariance matrix P. At each observation k, a 1×3 regressor row vector is formed:

```
φ_k = [ dω/dt_k,  ω_k,  1 ]
```

and the scalar observation is:

```
y_k = τ_motor_k
```

The prediction error is:

```
e_k = y_k − φ_k · θ_{k−1}
```

The Kalman gain vector is:

```
K_k = P_{k−1} · φ_kᵀ · (λ + φ_k · P_{k−1} · φ_kᵀ)⁻¹
```

The parameter and covariance updates are:

```
θ_k = θ_{k−1} + K_k · e_k
P_k = (P_{k−1} − K_k · φ_k · P_{k−1}) / λ
```

The scalar λ (0 < λ <= 1) is the **forgetting factor** (default 0.998). Values less than 1 cause older observations to carry exponentially decreasing weight, which is important for tracking slowly time-varying loads. At λ = 1 the estimator is an ordinary batch least squares — it never forgets.

The RLS state (θ and P) is held in a statically allocated object (`std::optional<MotorRLS>`) that is emplaced in-place at the start of each procedure invocation and destroyed when the procedure ends. This avoids heap allocation while still permitting the state to be absent between procedures.

```mermaid
flowchart TD
    START["EstimateFrictionAndInertia(Kt, polePairs, config, onDone)"] --> INIT["Initialise RLS\n(θ=0, P=αI)"]
    INIT --> CMD_SPEED["FocSpeed::SetPoint(target_speed)"]
    CMD_SPEED --> CB["ADC callback"]
    CB --> TAU["Estimate τ = Kt × Iq"]
    CB --> VEL["Estimate ω, dω/dt\nfrom encoder"]
    TAU --> RLS["RLS update step\n(φ=[dω/dt, ω, 1], y=τ)"]
    VEL --> RLS
    RLS --> TIMEOUT{"TimerSingleShot\nexpired?"}
    TIMEOUT -->|No| CB
    TIMEOUT -->|Yes| REPORT["Read θ = [J, B, τ_fric]"]
    REPORT --> CB2["onDone(B, J)"]
```

### Timeout and Result Delivery

A `TimerSingleShot` starts when the procedure begins. When it fires, the current RLS parameter vector is read and the results are extracted:

- **J** (inertia) is the first element of θ; reported as `NewtonMeterSecondSquared`.
- **B** (viscous friction) is the second element of θ; reported as `NewtonMeterSecondPerRadian`.
- **τ_friction** (Coulomb friction) is the third element of θ; it is observed internally but is not part of the output interface for this version.

If the estimation converges to physically implausible values (negative J, negative B), both outputs are reported as absent. No special "converged" criterion is enforced — the caller is responsible for treating results obtained at very low excitation or very short timeout as unreliable.

### State Machine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Running : EstimateFrictionAndInertia called
    Running --> TimedOut : TimerSingleShot fires
    TimedOut --> Idle : onDone(J, B) or onDone(nullopt) fired
```

Only one estimation may be in progress at a time. A call to `EstimateFrictionAndInertia` while already Running is rejected immediately (callback invoked with absent values).

---

## Interfaces

### Provided

| Interface                                                               | Purpose                                                                                                                                                              | Contract                                                                                                                         |
|-------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------|
| `EstimateFrictionAndInertia(torqueConstant, polePairs, config, onDone)` | Runs the RLS estimator under active speed control for a configurable duration; delivers `(optional<NewtonMeterSecondPerRadian>, optional<NewtonMeterSecondSquared>)` | Rejected (immediate failure callback) if already Running; fires exactly once; does not stop the speed control loop on completion |

### Required

| Interface            | Purpose                                                                                 | Contract                                                                       |
|----------------------|-----------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| `FocSpeed`           | Commands a target speed setpoint to excite the mechanical dynamics                      | Must already be active and in control of the motor before the procedure begins |
| `ThreePhaseInverter` | Source of ADC current callbacks that supply the Iq measurement on each computation step | Must not be stopped during the estimation procedure                            |
| `Encoder`            | Supplies mechanical angle samples for speed and acceleration estimation                 | Must be tracking position at the configured sampling rate                      |

---

## Online Inertia and Friction Estimation

In addition to the one-shot timed procedure above, a continuous online estimator (`RealTimeFrictionAndInertiaEstimator`) runs alongside the closed-loop speed controller to track slow mechanical parameter drift.

### Model

The mechanical equation of motion for the motor shaft is:

$$T_e = J \cdot \dot{\omega} + B \cdot \omega + \tau_c$$

The electromagnetic torque is approximated as $T_e = I_q \cdot k_t$, where $k_t$ is the torque constant (supplied by the calibration data). Angular acceleration $\dot{\omega}$ is derived from a finite difference of successive speed measurements scaled by the sampling frequency.

The regressor vector is $\phi = [1,\ \dot{\omega},\ \omega]^T$, and the parameter vector is $\theta = [\tau_c,\ J,\ B]^T$. The scalar output is $T_e$. An RLS algorithm with a forgetting factor of 0.995 updates $\theta$ each outer-loop period.

### Torque Constant Dependency

The torque constant $k_t$ must be provided before the estimator updates begin. In normal operation the state machine supplies $k_t$ from the calibration NVM record via `SetTorqueConstant()` during the `EnterEnabled` transition; updates run opportunistically while the FOC controller is active.

### RLS Self-Gating at Standstill

No explicit persistence-of-excitation gate is applied. When the motor is at standstill, $T_e \approx 0$, $\dot{\omega} \approx 0$, and $\omega \approx 0$, so the regressor norm is near zero, the prediction error is near zero, and the RLS update naturally produces negligible coefficient change. The estimator is therefore self-gating in the absence of excitation.

### Seeding and Warm Start

When calibration data is loaded (from NVM or after a fresh calibration run), the online estimator is seeded with the calibration values $(J_{cal}, B_{cal})$. This initialises the RLS coefficient vector to $\theta = [0,\ J_{cal},\ B_{cal}]^T$, setting the Coulomb-friction intercept to zero and warm-starting the inertia and viscous-friction estimates at the identified operating point. This avoids a cold-start transient where estimates are physically meaningless during the initial operating period.

### Forgetting Factor

The forgetting factor $\lambda = 0.995$ applies an exponential weight decay to past observations, enabling the estimator to track gradual mechanical changes over the motor lifetime (bearing wear increases friction; load changes affect effective inertia).

### Estimate Consumption

Estimates are not applied automatically. The operator or application explicitly triggers a speed-PID retune via `ApplyOnlineEstimates()` on the state machine. See the State Machine design document for details.
