---
title: "Electrical Parameters Identification — Resistance and Inductance"
type: theory
status: approved
version: 2.0.0
component: "service-electrical-ident"
date: 2026-07-11
---

| Field     | Value                                          |
|-----------|------------------------------------------------|
| Title     | Electrical Parameters Identification — R and L |
| Type      | theory                                         |
| Status    | approved                                       |
| Version   | 2.0.0                                          |
| Component | service-electrical-ident                       |
| Date      | 2026-07-11                                     |

## Overview

FOC performance depends directly on accurate stator resistance $R_s$ and inductance $L_s$. These
parameters are used to:
1. Design the PI current controller gains ($K_p = L_s \omega_{bw}$, $K_i = R_s \omega_{bw}$).
2. Implement feed-forward decoupling of the dq cross-coupling terms.
3. Estimate motor temperature from measured $R_s$ (since $R_s \propto T$).

This identification procedure applies a sequence of DC voltage steps to a single stator axis and
measures the resulting current. Because the rotor is stationary throughout, back-EMF is zero and the
excited axis behaves as a first-order RL circuit. Resistance is derived from a **multi-point
differential fit** ($\Delta V / \Delta I$) that cancels constant inverter and sensor offsets, and
inductance from the **integral of the current transient**. The excitation levels are **auto-scaled**
to the motor using a coarse pre-probe so that each level reaches a target fraction of the drive's
maximum current.

---

## Prerequisites

| Symbol     | Meaning                                             | Unit    |
|------------|-----------------------------------------------------|---------|
| $R_s$      | Stator resistance per phase                         | Ω       |
| $L_s$      | Stator inductance (d-axis, $L_d$)                   | H       |
| $\tau$     | Electrical time constant = $L_s / R_s$              | s       |
| $V_j$      | Applied step voltage at level $j$                   | V       |
| $I_{ss,j}$ | Steady-state current at level $j$                   | A       |
| $V_{err}$  | Inverter voltage error (fit intercept)              | V       |
| $V_{probe}$| Coarse pre-probe voltage                            | V       |
| $I_{ss}$   | Steady-state current of the probe transient         | A       |
| $f_s$      | Sampling frequency                                  | Hz      |
| $T_s$      | Sampling period = $1/f_s$                           | s       |
| $N_{levels}$ | Number of $\Delta V/\Delta I$ levels               | —       |
| $N_{buf}$  | Probe transient buffer size                         | samples |

---

## Mathematical Foundation

### 1. Single-Axis Excitation and Back-EMF Suppression

The procedure energises one stator axis with a DC field (high duty on phase A, neutral on B and C).
Because every step is a DC level, the rotor is **stationary** at the moment of measurement, so:

- The electrical speed is $\omega_e = 0$, hence the back-EMF $e = \psi_f \omega_e = 0$.
- Only the excited RL circuit carries current.

The rotor is pulled into alignment with the applied field during the coarse pre-probe and the first
graduated levels. Because every level shares the **same** stator axis, the equilibrium angle never
changes between levels — so the rotor does not move during the transient used for inductance. No
explicit alignment routine is required; a poor regression fit (Section 3) flags any residual motion.

The excited-axis circuit model reduces to:

$$
v = R_s\, i + L_s \frac{di}{dt}
$$

This is a first-order linear system driven by a step of amplitude $V$.

### 2. RL Step Response

For a step input $v(t) = V \cdot u(t)$ with zero initial conditions ($i(0) = 0$):

$$
\boxed{i(t) = \frac{V}{R_s}\!\left(1 - e^{-t/\tau}\right)}, \qquad \tau = \frac{L_s}{R_s}
$$

Key properties of this response:
- At $t = \tau$: $i(\tau) = I_{ss}(1 - e^{-1}) \approx 0.6321 \cdot I_{ss}$
- At $t = 5\tau$: $i(5\tau) \approx 0.9933 \cdot I_{ss}$ (essentially settled)
- Slope at $t = 0$: $\left.\frac{di}{dt}\right|_{t=0} = \frac{V}{L_s}$

### 3. Resistance Estimation — Multi-Point Differential Fit

A single-point estimate $R_s = V/I_{ss}$ bakes every constant error — inverter dead-time, MOSFET and
body-diode drops, and current-sensor DC offset — directly into $R_s$. Instead, $N_{levels}$ steps are
applied and the steady-state pairs $(I_{ss,j}, V_j)$ are fit by ordinary least squares to a line:

$$
V_j = R_s\, I_{ss,j} + V_{err}
$$

- The **slope** $R_s$ is immune to any constant voltage error or current offset — they cancel in the
  differential $\Delta V/\Delta I$.
- The **intercept** $V_{err}$ estimates the total inverter voltage error, exported as free diagnostic
  data (usable later for dead-time compensation).

**Auto-scaling.** A coarse pre-probe at $V_{probe}$ yields $R_{coarse} = V_{probe}/I_{probe}$. Each
level then targets a current fraction $f_j$ of the drive maximum $I_{max}$, choosing the duty so that
$V_j \approx f_j\, I_{max}\, R_{coarse}$ (clamped to a safe duty range). This keeps the currents high
enough to escape the worst dead-time non-linearity regardless of the motor.

**Fit quality.** The maximum normalised residual
$\max_j |V_j - (R_s I_{ss,j} + V_{err})| / R_s$ is reported. A large value indicates the $V$–$I$
relationship was not linear — typically rotor motion or ADC saturation — and the estimate is
rejected.

**Winding topology.** For a Delta connection the terminals measure $\tfrac{2}{3}$ of the per-phase
value for both resistance and inductance; the phase quantities are recovered with
$R_\phi = R_{terminal} \cdot k_\Delta$ and $L_\phi = L_{terminal} \cdot k_\Delta$, $k_\Delta = 1.5$.

### 4. Inductance Estimation — Integral Method

Rather than locating the 63.2% threshold crossing (which quantises $\tau$ to one sample and is
sensitive to a single noisy point), the inductance is obtained from the integral identity of a
first-order rise. For $i(t) = I_{ss}(1 - e^{-t/\tau})$:

$$
\int_0^\infty \bigl(I_{ss} - i(t)\bigr)\,dt = I_{ss}\,\tau
\quad\Longrightarrow\quad
\boxed{L_s = R_s \cdot \frac{\displaystyle\sum_k \bigl(I_{ss} - i[k]\bigr)\,T_s}{I_{ss}}}
$$

The sum runs over the full probe transient (from step onset to plateau). Properties:

- **Every sample contributes**, so noise averages out and the result has sub-sample-period
  resolution — it removes both weaknesses of the threshold method.
- The integrand is a **difference** $(I_{ss} - i[k])$, so any constant current-sensor offset cancels.
- The probe step starts from near-zero current, so the denominator is the probe $I_{ss}$ and $R_s$ is
  the value fitted in Section 3.

**Requirement**: the buffer must span the transient to a true plateau ($N_{buf} \gtrsim 5\tau/T_s$).
If $\tau$ is large relative to the buffer, increase $N_{buf}$ or the probe voltage.

### 5. Pole Pair Estimation

The number of electrical cycles per mechanical revolution equals the number of pole pairs $p$.
During the multi-step alignment sweep, the motor is driven through exactly 12 electrical steps over
one full electrical revolution ($2\pi$ electrical). The encoder counts $C_{mech}$ per step multiplied
by $N_{steps} = 12$ gives the total encoder counts per full electrical cycle. Dividing by the known
encoder counts per mechanical revolution $C_{rev}$ gives:

$$
p = \frac{C_{rev}}{12 \cdot C_{per\_step}} \quad \text{(integer, rounded)}
$$

or equivalently, the total electrical angle traversed over the full 12-step sweep spans exactly
$2\pi$ electrical = $2\pi/p$ mechanical, so:

$$
p = \frac{2\pi}{\Delta\theta_{mech,total}}
$$

where $\Delta\theta_{mech,total}$ is the measured total mechanical rotation during the sweep.

### 6. Complete Identification Sequence

```
1. Coarse pre-probe: apply V_probe on one axis, collect the full transient,
   take I_probe from the last 10% -> R_coarse = V_probe / I_probe.
   (This step also settles/aligns the rotor to the excited axis.)

2. For each level j in [0 .. N_levels-1]:
     a. Auto-scale duty so the current targets f_j * I_max (using R_coarse).
     b. Settle for settlePerLevel, then average a steady-state batch -> I_ss_j.
   Record the applied voltage V_j.

3. Fit V_j = R_s * I_ss_j + V_err by least squares.
   Reject if any I_ss_j is near zero, if the slope R_s <= 0, or if the
   normalised residual exceeds the fit-quality threshold.

4. Inductance from the probe transient integral:
   L_s = R_s * sum((I_ss - i[k]) * T_s) / I_ss.

5. Apply the Delta winding correction (k = 1.5) to both R_s and L_s when configured.

6. Report { R_s, L_s, V_err, fit_quality }.
```

---

## Block Diagrams

### Electrical Identification Signal Flow

```mermaid
graph TD
    A[Coarse pre-probe\nV_probe on one axis] --> B[R_coarse = V_probe / I_probe\nrotor settles on axis]
    B --> C[For each level j:\nauto-scale duty to f_j * I_max]
    C --> D[Settle + average\nI_ss_j]
    D --> E{More levels?}
    E -- yes --> C
    E -- no --> F[LS fit\nV_j = R_s I_ss_j + V_err]
    F --> G[Integral method\nL_s = R_s Σ(I_ss − i)Ts / I_ss]
    F --> H[Fit quality\n+ Delta correction]
    G --> I[Report R_s, L_s, V_err, quality]
    H --> I
```

### RL Step Response — ASCII Approximation

```
i_d (normalised: I_ss = 1.0)
  │
1.0├───────────────────────────────── I_ss = V/R (steady state)
   │                       ──────────
0.86├──────────────────────/
   │                     /
0.63├────────────────────/ ← i(τ) = 0.632·I_ss
   │                   /
   │                  /
0.39├─────────────────/
   │               /
   │             /
   │           /
0.0├───────────
   └───────────────────────────────── samples (n·T_s)
       0      τ/T_s   2τ/T_s  5τ/T_s

The whole shaded area between I_ss and i(t) is integrated: L_s = R_s · area / I_ss.
```

---

## Numerical Properties

| Property             | Value / Condition                                            |
|----------------------|--------------------------------------------------------------|
| Sampling rate        | $f_s = 10\ \text{kHz}$, $T_s = 100\ \mu\text{s}$             |
| Probe buffer         | $N_{buf} = 512$ samples ($51.2\ \text{ms}$)                 |
| Resistance levels    | $N_{levels} = 3$ (default fractions $0.3, 0.5, 0.7$)        |
| Steady-state batch   | $32$ samples per level                                       |
| Threshold            | integral method — no fixed threshold                        |
| $R_s$ range          | Nominally $0.1\ \Omega$ to $50\ \Omega$ (ADC current range)  |
| $L_s$ resolution     | sub-sample (integral of the full transient)                 |
| Fit-quality gate     | reject if normalised residual $> 0.1$                       |
| Trigger voltage      | auto-scaled to target current fraction of $I_{max}$          |

### Sensitivity Analysis

| Source of Error            | Effect on $R_s$                          | Effect on $L_s$                          |
|----------------------------|------------------------------------------|------------------------------------------|
| ADC current offset         | Cancelled by the differential fit        | Cancelled in the integrand difference    |
| Inverter dead-time / drops | Cancelled (lands in $V_{err}$ intercept) | Indirect via $R_s$ error                 |
| $V_{dc}$ variation         | Biases $V_j$ (kept brief to limit drift) | Indirect via $R_s$ error                 |
| Thermal drift in $R_s$     | Measurement valid at $T_{meas}$ only     | —                                        |
| Rotor motion in transient  | Flagged by fit-quality residual          | Corrupts integral; rejected if flagged   |
| Insufficient buffer        | —                                        | $\tau$ / area underestimated             |
| Magnetic saturation        | $R_s$ underestimated                     | $L_s$ underestimated (nonlinear)         |

---

## Worked Example

Motor: $R_s = 1.2\ \Omega$, $L_s = 0.6\ \text{mH}$, $f_s = 10\ \text{kHz}$,
three levels at $V_1 = 1.2\,\text{V}$, $V_2 = 2.0\,\text{V}$, $V_3 = 2.8\,\text{V}$ with a
constant inverter error $V_{err} = 0.3\,\text{V}$.

**Steady-state currents** ($I_{ss,j} = (V_j - V_{err})/R_s$):

$$I_{ss,1} = 0.75\,\text{A},\quad I_{ss,2} = 1.417\,\text{A},\quad I_{ss,3} = 2.083\,\text{A}$$

**Resistance** — the least-squares slope through the three points:

$$R_s = \frac{\Delta V}{\Delta I} = \frac{2.8 - 1.2}{2.083 - 0.75} = 1.2\ \Omega,\qquad V_{err} = 0.3\,\text{V}$$

A single-point estimate at level 1 would instead give $1.2/0.75 = 1.6\ \Omega$ — a **33% error** —
showing why the differential fit matters.

**Inductance** — integrating the probe transient ($\tau = L_s/R_s = 0.5\,\text{ms} = 5\,T_s$):

$$L_s = R_s \cdot \frac{\sum_k (I_{ss} - i[k])\,T_s}{I_{ss}} = R_s \cdot \tau = 1.2 \times 0.5\times10^{-3} = 0.6\ \text{mH}$$

The integral recovers $\tau$ with sub-sample resolution, independent of any single noisy point.

---

## Limitations & Assumptions

- **Assumes**: The rotor is stationary during each transient. The pre-probe and same-axis graduated
  steps ensure this; residual motion is caught by the fit-quality residual.
- **Assumes**: $L_d \approx L_q$ (surface-mounted PMSM). For interior PMSM, the step must be
  repeated on both axes if the design requires both $L_d$ and $L_q$.
- **Assumes**: Magnetic linearity (no saturation). The identification current must be kept below the
  saturation current.
- **Does not handle**: Temperature-dependent $R_s$ variation during operation.
- **Does not handle**: Identification at running speed where back-EMF cannot be zeroed by standstill.

## References

1. Rauf, A. et al. — "Online Identification of PMSM Parameters Based on Extended Kalman Filter",
   *IEEE Transactions on Industrial Electronics*, 2019.
2. Underwood, S.J. & Husain, I. — "Online Parameter Estimation and Adaptive Control of PMSM",
   *IEEE Transactions on Industrial Electronics*, 2010.
3. Texas Instruments Application Report SPRABV5 — *Motor Control in Embedded Applications*, 2018.
