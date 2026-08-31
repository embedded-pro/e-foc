---
title: "Speed Loop — S1: LQI"
type: theory
status: draft
version: 1.0.0
component: "speed-loop-lqi"
date: 2026-08-31
---

| Field     | Value                |
|-----------|----------------------|
| Title     | Speed Loop — S1: LQI |
| Type      | theory               |
| Status    | draft                |
| Version   | 1.0.0                |
| Component | speed-loop-lqi       |
| Date      | 2026-08-31           |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

S1 (LQI — Linear-Quadratic-Integral) state-feedback computes gains that minimise an explicit cost
function directly from the mechanical RLS estimates $\hat{J}$ and $\hat{B}_f$. A PI speed controller
is heuristically tuned and its gains have no direct physical meaning. LQI provides systematic tuning:
the weights $q_\omega$, $q_I$, and $R$ have transparent physical interpretations. The integral
augmentation guarantees zero steady-state error to a constant speed reference.

Operates exclusively in the **1 kHz outer handler**. Requires mechanical RLS convergence ($J$, $B_f$).

---

## Prerequisites

| Symbol         | Meaning                            | Unit |
|----------------|------------------------------------|------|
| $A_d^o, B_d^o$ | Discrete speed plant matrices      | —    |
| $Q, R$         | LQR weighting matrices             | —    |
| $P$            | DARE solution (optimal cost-to-go) | —    |
| $I_{q,max}$    | Maximum $q$-axis current           | A    |

See `documentation/theory/foc-plant-models.md` §2 for the speed plant derivation.

---

## Mathematical Foundation

All speed controllers operate on the **discrete mechanical speed plant** from
`documentation/theory/foc-plant-models.md` §2. Treating the current loop as ideal
($i_q \approx i_q^*$), the speed dynamics under control input $u = i_q^*$ are:

$$
\omega_m[k+1] = A_d^o \cdot \omega_m[k] + B_d^o \cdot u[k], \quad
A_d^o \approx 1 - \frac{B_f T_s^o}{J},\quad B_d^o = \frac{K_t T_s^o}{J}
$$

---

## S1 — LQI Speed Control

**Integral augmentation**: Augment the discrete speed plant with an integrator state $x_I$ that
accumulates the speed error:

$$
x_I[k+1] = x_I[k] + (\omega_m^*[k] - \omega_m[k])
$$

The augmented plant matrices:

$$
\mathbf{A}_{aug} = \begin{pmatrix} A_d^o & 0 \\ -1 & 1 \end{pmatrix},
\qquad
\mathbf{B}_{aug} = \begin{pmatrix} B_d^o \\ 0 \end{pmatrix}
$$

**Gain design via DARE**:

$$
P = A_{aug}^T P A_{aug} - A_{aug}^T P B_{aug}(R + B_{aug}^T P B_{aug})^{-1} B_{aug}^T P A_{aug} + Q
$$

$$
\mathbf{K} = [K_\omega,\; K_I] = (R + B_{aug}^T P B_{aug})^{-1} B_{aug}^T P A_{aug}
$$

**Control law** (run-time: two multiplications and one addition):

$$
u[k] = -K_\omega\,(\omega_m[k] - \omega_m^*[k]) - K_I\, x_I[k]
$$

Structurally identical to a discrete PI but with gains derived from physical parameters and an
explicit performance cost.

**Tuning** ($Q = \mathrm{diag}(q_\omega, q_I)$, scalar $R$):
- $q_\omega$: speed error penalty — tighter tracking, higher control effort.
- $q_I$: integral state penalty — faster integrator wind-down, less overshoot.
- $R$: input penalty — directly caps the $i_q^*$ command.
- Starting point: $q_\omega = 1$, $q_I = 0.1$, $R = 1/I_{q,max}^2$.

**Gain computation** runs once at configuration time off the hot path using the `DARE` solver from
the numerical toolbox. The 1 kHz handler executes only the dot product.

**Numerical conditioning**: the DARE is solved with the control input expressed per-unit of
$I_{q,max}$ and with the integral state accumulating the raw error sum rather than $e \cdot T_s^o$.
Both substitutions leave $\mathbf{K}$ unchanged — only the ratio $Q/R$ and the state scaling matter —
but they keep the entries of $P$ of order unity, which the single-precision fixed-point iteration
needs in order to reach its absolute convergence threshold.

**Anti-windup**: the integral state is frozen (conditional integration) on any sample where the
unsaturated command falls outside $\pm I_{q,max}$, and resumes as soon as the command re-enters the
envelope.

---

## Numerical Properties

| Property                   | Value                                           |
|----------------------------|-------------------------------------------------|
| Ops per 1 kHz cycle        | 2 MACs (dot product $\mathbf{K} \cdot x_{aug}$) |
| Steady-state speed error   | Zero (integral augmentation)                    |
| Load disturbance rejection | PI quality (integral-based)                     |
| Tracking vs. stiffness     | Coupled (single DOF)                            |
| DARE timing                | < 2 µs at 120 MHz, once at config time          |
| Requires mechanical RLS    | Yes ($J$, $B_f$)                                |
| Tuning knobs               | 3 ($q_\omega$, $q_I$, $R$)                      |

---

## Limitations & Assumptions

- Treats the current loop as ideal ($i_q \approx i_q^*$). Valid when current-loop bandwidth is at
  least 10× the speed-loop bandwidth.
- Integral wind-up: the integral state $x_I$ must be clamped when $i_q^*$ saturates. Anti-windup is
  required.
- DARE conditioning: if $B_d^o = K_t T_s^o / J \approx 0$ (extremely large inertia), the DARE may
  be ill-conditioned. Verify the controllability Gramian before solving.
- Speed estimate must be smooth. A noisy encoder derivative degrades all speed controllers equally.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
