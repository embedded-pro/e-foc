---
title: "Speed Loop — S3: Two-DOF"
type: theory
status: draft
version: 1.0.0
component: "speed-loop-two-dof"
date: 2026-08-31
---

| Field     | Value                  |
|-----------|------------------------|
| Title     | Speed Loop — S3: Two-DOF |
| Type      | theory                 |
| Status    | draft                  |
| Version   | 1.0.0                  |
| Component | speed-loop-two-dof     |
| Date      | 2026-08-31             |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

S3 (Two-DOF speed control) resolves a fundamental limitation of single-DOF speed controllers:
tightening the feedback loop for faster setpoint following degrades load stiffness and vice versa.
A reference pre-filter $F(z)$ in the forward path shapes command tracking independently from the
feedback loop designed for disturbance rejection. This is particularly valuable for servo speed
control where both fast tracking and high stiffness are required simultaneously.

Operates exclusively in the **1 kHz outer handler**. No mechanical RLS required.

---

## Prerequisites

| Symbol             | Meaning                                  | Unit |
|--------------------|------------------------------------------|------|
| $A_d^o, B_d^o$     | Discrete speed plant matrices            | —    |
| $\tau_{ff}$        | Two-DOF pre-filter time constant         | s    |
| $K_p$, $K_i$       | PI feedback controller gains             | —    |

See `documentation/theory/foc-plant-models.md` §2 for the speed plant derivation.
See `documentation/theory/speed-loop-pi.md` for PI gain design.

---

## Mathematical Foundation

All speed controllers operate on the **discrete mechanical speed plant** from
`documentation/theory/foc-plant-models.md` §2:

$$
\omega_m[k+1] = A_d^o \cdot \omega_m[k] + B_d^o \cdot u[k]
$$

S3 adds a reference pre-filter $F(z)$ in the forward path that shapes tracking independently from
the feedback loop designed for disturbance stiffness.

---

## S3 — Two-DOF Speed Control

**Structure**: The PI feedback controller receives a pre-filtered setpoint:

$$
e_{fb}[k] = F(z)\,\omega_m^*[k] - \omega_m[k]
$$

The pre-filter is a first-order low-pass parameterised by $\tau_{ff}$:

$$
F(z) = \frac{1 - e^{-T_s^o/\tau_{ff}}}{z - e^{-T_s^o/\tau_{ff}}}
$$

**Decoupled transfer functions**:

$$
T_{track}(z) = F(z)\,C_{fb}(z)\,G(z)\,(1 + C_{fb}(z)\,G(z))^{-1}
$$
$$
T_{dist}(z) = G(z)\,(1 + C_{fb}(z)\,G(z))^{-1}
$$

$\tau_{ff}$ shapes tracking speed; PI gains shape disturbance rejection. Neither constrains the other.

**Relation to toolbox**: an `ExponentialMovingAverage` realises $F(z)$ and an incremental PI provides
$C_{fb}(z)$, with $\alpha = 1 - e^{-T_s^o/\tau_{ff}}$ mapping the pre-filter time constant onto the
filter.

**Tuning**:
- PI gains ($K_p$, $K_i$): design for disturbance rejection bandwidth $\omega_{bw}$ as in standard PI
  (see `documentation/theory/speed-loop-pi.md`).
- $\tau_{ff}$: set $\leq 1/\omega_{bw}$ for tracking at least as fast as the feedback loop. Set
  larger to deliberately smooth abrupt setpoint changes.

---

## Numerical Properties

| Property                   | Value                                          |
|----------------------------|------------------------------------------------|
| Ops per 1 kHz cycle        | ~8 MACs (pre-filter + PI)                      |
| Steady-state speed error   | Zero (PI integral action)                      |
| Load disturbance rejection | PI quality                                     |
| Tracking vs. stiffness     | Decoupled ($\tau_{ff}$ independent of PI gains)|
| Requires mechanical RLS    | No                                             |
| Tuning knobs               | 3 ($K_p$, $K_i$, $\tau_{ff}$)                 |

---

## Limitations & Assumptions

- Treats the current loop as ideal ($i_q \approx i_q^*$). Valid when current-loop bandwidth is at
  least 10× the speed-loop bandwidth.
- $\tau_{ff}$ must be smaller than the closed-loop mechanical time constant to avoid pre-filter lag
  causing overshoot.
- If the incoming speed reference is already smooth, $\tau_{ff}$ can equal the PI time constant,
  which degenerates the Two-DOF to a standard PI.
- Speed estimate must be smooth. A noisy encoder derivative degrades all speed controllers equally.

---

## References

1. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
2. Åström, K.J. & Hägglund, T. — *Advanced PID Control*, ISA, 2006.
   (Two-DOF PID structure, reference pre-filter design.)
