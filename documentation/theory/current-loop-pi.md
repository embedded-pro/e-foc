---
title: "Current Loop — PI (Baseline)"
type: theory
status: draft
version: 1.0.0
component: "current-loop-pi"
date: 2026-08-31
---

| Field     | Value                        |
|-----------|------------------------------|
| Title     | Current Loop — PI (Baseline) |
| Type      | theory                       |
| Status    | draft                        |
| Version   | 1.0.0                        |
| Component | current-loop-pi              |
| Date      | 2026-08-31                   |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

The baseline current controller is a discrete incremental PI regulator applied independently to the
d- and q-axis current errors in the rotating dq frame. It is always available — no RLS convergence
is required — and its pole-zero cancellation design places the closed-loop bandwidth precisely at the
specified value $\omega_{bw}^i$. All advanced current controllers (A1–A3) supplement or replace this
baseline while preserving the same output interface.

Operates exclusively in the **20 kHz FOC ISR**.

---

## Prerequisites

| Symbol         | Meaning                                   | Unit  |
|----------------|-------------------------------------------|-------|
| $R_s$          | Stator resistance per phase               | Ω     |
| $L_s$          | Stator inductance ($L_d = L_q$)           | H     |
| $V_{dc}$       | DC bus voltage (measured dynamically)     | V     |
| $T_s^i$        | Current loop sample period ($= 50\ \mu$s) | s     |
| $\omega_{bw}$  | Desired closed-loop current bandwidth     | rad/s |
| $A_d^i, B_d^i$ | Discrete RL plant matrices                | —     |

See `documentation/theory/foc-plant-models.md` for all base symbols and ZOH derivations.

---

## Mathematical Foundation

### Plant Model

After feedforward decoupling the cross-coupling terms (see `documentation/theory/foc.md` §4), each
dq axis reduces to an independent first-order RL plant with decoupled input $v'$:

$$
\frac{di}{dt} = -\frac{R_s}{L_s}\, i + \frac{1}{L_s}\, v'
$$

ZOH-discretized at $T_s^i$ (from `documentation/theory/foc-plant-models.md` §1):

$$
i[k+1] = A_d^i\, i[k] + B_d^i\, v'[k], \qquad
A_d^i = e^{-R_s T_s^i / L_s},\quad B_d^i = \frac{1 - A_d^i}{R_s}
$$

### PI Gain Design — Pole-Zero Cancellation

The continuous-time RL plant has a single pole at $s = -R_s/L_s$. A PI controller:

$$
C(s) = K_p \cdot \frac{s + K_i/K_p}{s}
$$

achieves pole-zero cancellation by placing the PI zero at the plant pole:

$$
\frac{K_i}{K_p} = \frac{R_s}{L_s}
$$

The closed-loop transfer function then reduces to a first-order system with bandwidth $\omega_{bw}$:

$$
K_p = L_s\, \omega_{bw}, \qquad K_i = R_s\, \omega_{bw}
$$

**Practical limit**: $\omega_{bw} \leq \pi / T_s^i$ (Nyquist). The maximum achievable bandwidth
at 20 kHz is roughly $\omega_{bw} \leq 10000$ rad/s; conservative designs use $2000$–$5000$ rad/s.

### Gain Normalisation

The modulator accepts per-unit voltages where $1\ \text{pu} = V_{dc}/\sqrt{3}$. Physical PI gains
must be scaled before being applied to the normalised output:

$$
K_p^{norm} = K_p \cdot \frac{\sqrt{3}}{V_{dc}} = L_s\, \omega_{bw} \cdot \frac{\sqrt{3}}{V_{dc}}
$$

$V_{dc}$ is measured dynamically and used to update normalisation in real time. Without this scaling
the effective bandwidth changes with bus voltage.

### Incremental (Velocity) Form

The implementation uses the incremental (velocity) PI form to avoid integrator initialisation
issues. Each axis proposes

$$
\Delta v'[k] = K_p \cdot \bigl(e[k] - e[k-1]\bigr) + K_i\, T_s^i\, e[k]
$$
$$
\tilde{v}'[k] = v'[k-1] + \Delta v'[k]
$$

where $v'[k-1]$ is the value the actuator **actually applied** on the previous sample, not the
value this axis proposed.

### Anti-Windup

The actuator constraint here is the modulation circle (see *Output Voltage Limit* below). It couples
$d$ and $q$, so it cannot be expressed as the independent per-axis clamp a stock PID carries: a
per-axis clamp to $[-1,1]$ never fires for a demand such as $v_d' = v_q' = 0.8$, yet that vector has
magnitude $1.13$ and the circle limiter scales it down. Both integrators would then accumulate
against a limit they cannot see.

The loop therefore uses back-calculation against the realised output. Both axes propose, the circle
limit is applied once to the resulting vector, and the applied components are committed back as
$v'[k-1]$ for the next sample:

$$
v'[k] = \Pi\bigl(\tilde{v}'_d[k],\ \tilde{v}'_q[k]\bigr)
$$

where $\Pi$ is the projection onto the unit disc. When the demand is inside the circle the
projection is the identity and the law reduces to the plain velocity form.

---

## Output Voltage Limit

Every current controller in this loop emits a normalised dq voltage pair passed to inverse Park and
then to SVM. The modulator remains linear only inside the circle inscribed in the voltage hexagon
(`documentation/theory/foc.md` §6):

$$
\sqrt{(v_d')^2 + (v_q')^2} \leq 1
\qquad \Longleftrightarrow \qquad
\sqrt{v_d^2 + v_q^2} \leq \frac{V_{dc}}{\sqrt{3}}
$$

This is a **circular** constraint on the vector, not an independent per-axis bound. When the
vector demand exceeds the unit circle, both components are scaled by $1/\lVert v' \rVert$,
preserving voltage angle (and therefore current vector direction) while sacrificing magnitude.

Clamping $v_d'$ and $v_q'$ separately to $[-1,1]$ admits vectors up to magnitude $\sqrt{2}$,
driving the modulator into over-modulation over roughly three-quarters of the electrical period.

---

## Numerical Properties

| Property                  | Value                                    |
|---------------------------|------------------------------------------|
| ISR cost                  | ~6 MACs (2 axes × 3 operations)          |
| Settling time             | ~$1/\omega_{bw}$                         |
| Robustness to Rs/Ls error | High — integral action corrects mismatch |
| Requires RLS convergence  | No — motor datasheet values sufficient   |
| Requires $\psi_f$         | No                                       |
| Tuning knobs              | 1 ($\omega_{bw}$) + 1 ($V_{dc}$ live)    |

---

## Limitations & Assumptions

- Cross-coupling terms $\omega_e L_s i_q$ and $\omega_e L_s i_d$ are treated as disturbances.
  At high electrical speed $\omega_e$, the coupling voltage reduces effective bandwidth and introduces
  cross-axis torque ripple. Use A1 (Decoupled PID) to cancel this at speed.
- Assumes balanced three-phase operation ($i_a + i_b + i_c = 0$).
- Requires accurate electrical angle $\theta_e$. Angle errors couple directly into the dq frame.
- Output subject to the circular SVM constraint — see *Output Voltage Limit* above.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
3. Holmes, D.G. & Lipo, T.A. — *Pulse Width Modulation for Power Converters*, IEEE Press, 2003.
