---
title: "Position Loop — P3: Two-DOF"
type: theory
status: draft
version: 1.0.0
component: "position-loop-two-dof"
date: 2026-08-31
---

| Field     | Value                       |
|-----------|-----------------------------|
| Title     | Position Loop — P3: Two-DOF |
| Type      | theory                      |
| Status    | draft                       |
| Version   | 1.0.0                       |
| Component | position-loop-two-dof       |
| Date      | 2026-08-31                  |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

P3 (Two-DOF position control) eliminates the fundamental tracking vs. stiffness tradeoff inherent
in single-DOF position controllers. The reference pre-filter $F(z)$ shapes how the position setpoint
is approached, independently from the feedback loop designed for load-disturbance stiffness. This is
the generalisation of Cascade P (P2): when $F(z) = 1$ and the feedback is proportional, P3 collapses
to P2.

Operates exclusively in the **1 kHz outer handler**. No parameter identification required.

---

## Prerequisites

| Symbol         | Meaning                          | Unit |
|----------------|----------------------------------|------|
| $\tau_{ff}$    | Two-DOF pre-filter time constant | s    |
| $A_d^p, B_d^p$ | Discrete position plant matrices | —    |

See `documentation/theory/foc-plant-models.md` §3 for the position plant derivation.
See `documentation/theory/position-loop-cascade-p.md` for the Cascade P special case.

---

## Mathematical Foundation

All position-loop controllers operate on the **discrete two-state position plant** from
`documentation/theory/foc-plant-models.md` §3. P3 is the Two-DOF generalisation of the single
position-loop feedback controller: a reference pre-filter $F(z)$ decouples tracking bandwidth
from disturbance stiffness bandwidth.

---

## P3 — Two-DOF Position Control

**Structure**: A PD or LQR feedback controller acts on the pre-filtered position error:

$$
e_{fb}[k] = F(z)\,\theta_m^*[k] - \theta_m[k]
$$

**Wrapping constraint**: $\theta_m$ lives on the circle, not on the line, so $F(z)$ must not be run
directly on the wrapped angle: filtering from $+3.0$ rad towards $-3.0$ rad would interpolate through
$0$ and execute a $344^\circ$ rotation instead of the $16^\circ$ move across the seam. The
first-order prefilter is therefore realised on the wrapped error:

$$
\boxed{\theta_f[k] = \mathrm{wrap}\Bigl(\theta_f[k-1] + \alpha \cdot \mathrm{wrap}\bigl(\theta_m^*[k] - \theta_f[k-1]\bigr)\Bigr)}
$$

with $\alpha = 1 - e^{-T_o/\tau_{ff}}$ and $\mathrm{wrap}(\cdot)$ folding onto $(-\pi, \pi]$. The state
$\theta_f$ is seeded from the measured angle on reset and on the first sample; seeding it at zero
would ramp the reference in from absolute zero on every enable. For $\tau_{ff} \le 0$ the prefilter
is transparent and the raw setpoint passes through.

**Relation to P2**: When $F(z) = 1$ and the feedback controller is a proportional P, the structure
collapses to Cascade P→PI. Two-DOF position control is the generalisation — $F(z)$ independently
shapes tracking and the feedback controller independently shapes stiffness.

**Relation to toolbox**: `Feedforward2Dof` implements this structure.

**Tuning**: $\tau_{ff}$ for tracking speed; feedback gains for disturbance rejection. As S3.

---

## Numerical Properties

| Property                    | Value                                           |
|-----------------------------|-------------------------------------------------|
| Ops per 1 kHz cycle         | ~8 MACs (pre-filter + feedback controller)      |
| Steady-state position error | Configurable (depends on feedback controller)   |
| Tracking vs. stiffness      | Decoupled ($\tau_{ff}$ independent of feedback) |
| Requires J, Bf              | No                                              |
| Tuning knobs                | $\tau_{ff}$ + feedback controller gains         |

---

## Limitations & Assumptions

- Treat speed loop as ideal. P3 requires a properly tuned inner speed loop.
- Pre-filter pole must be faster than the desired closed-loop position bandwidth. Verify that
  $1/\tau_{ff} > \omega_{pos}$.
- The wrapping realisation is mandatory — direct application of $F(z)$ to the angle signal is
  numerically incorrect for setpoints that cross the $\pm\pi$ seam.
- Encoder noise enters position and velocity estimates. A state observer upstream improves robustness.

---

## References

1. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
2. Åström, K.J. & Hägglund, T. — *Advanced PID Control*, ISA, 2006.
   (Two-DOF PID structure, reference pre-filter design.)
