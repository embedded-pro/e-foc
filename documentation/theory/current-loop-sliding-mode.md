---
title: "Current Loop — A3: Sliding-Mode"
type: theory
status: draft
version: 1.0.0
component: "current-loop-sliding-mode"
date: 2026-08-31
---

| Field     | Value                            |
|-----------|----------------------------------|
| Title     | Current Loop — A3: Sliding-Mode  |
| Type      | theory                           |
| Status    | draft                            |
| Version   | 1.0.0                            |
| Component | current-loop-sliding-mode        |
| Date      | 2026-08-31                       |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

A3 (Sliding-Mode) is robust to model uncertainty — the most critical early in operation before
RLS has converged and during thermal transients that shift $R_s$. The SMC drives the current error
onto a stable sliding surface regardless of parameter mismatch, within a defined gain margin.
Unlike A1 and A2, A3 deliberately omits the A1 feedforward and treats residual coupling and noise
as a bounded disturbance covered by the switching gain $K_{sw}$.

Operates exclusively in the **20 kHz FOC ISR**. Partial RLS dependency — $A_d^i$, $B_d^i$ are
used but the controller remains stable over a wide range of mismatch.

---

## Prerequisites

| Symbol        | Meaning                                  | Unit |
|---------------|------------------------------------------|------|
| $A_d^i, B_d^i$| Discrete current plant matrices          | —    |
| $\phi$        | Sliding-mode boundary layer width        | A    |
| $K_{sw}$      | Sliding-mode switching gain              | A    |

See `documentation/theory/foc-plant-models.md` for ZOH derivations.

---

## Mathematical Foundation

All current controllers operate on the **decoupled per-axis RL plant** derived in
`documentation/theory/foc-plant-models.md` §1:

$$
i[k+1] = A_d^i \cdot i[k] + B_d^i \cdot v'[k]
$$

A3 deliberately omits the A1 feedforward and treats the residual coupling and noise as a bounded
disturbance covered by the switching gain $K_{sw}$; that robustness is the reason to select it
before RLS has converged.

---

## A3 — Sliding-Mode Current Control

**Sliding surface**: For each axis independently, defined on the tracking error
$e = i - i^*$:

$$
s_d = i_d - i_d^*, \qquad s_q = i_q - i_q^*
$$

The surface $s = 0$ is the desired zero-error manifold.

**Discrete equivalent control**: The voltage that would maintain $s[k+1] = 0$. With the plant
$i[k+1] = A_d^i i[k] + B_d^i u[k]$ the error propagates as
$e[k+1] = A_d^i e[k] + B_d^i u[k] + (A_d^i - 1) i^*[k]$, so holding the surface needs the error
term *and* the voltage that sustains the reference itself:

$$
\boxed{u_{eq}[k] = -\frac{A_d^i}{B_d^i} \cdot e[k] + \frac{1 - A_d^i}{B_d^i} \cdot i^*[k]}
$$

Because $B_d^i = (1 - A_d^i)/R_s$, the second term is exactly $R_s i^*[k]$ — the resistive drop
the reference current requires. Omitting it leaves a standing error
$e_\infty = (A_d^i - 1) i^* / (1 + K_{sw}/\phi)$, which this controller has no integral action to
remove.

**Switching control**: Drives the state onto the surface:

$$
u_{sw}[k] = -\frac{K_{sw}}{B_d^i} \cdot \mathrm{sat}\!\left(\frac{e[k]}{\phi}\right)
$$

where $\mathrm{sat}(x) = \mathrm{clamp}(x,-1,1)$. The boundary layer $\phi > 0$ replaces the
discontinuous sign function with saturation to prevent chattering. Both terms carry a leading minus
sign because the error is measured as $i - i^*$: a current below its reference gives $e < 0$ and
must command a *positive* voltage.

**Total control law**:

$$
u[k] = u_{eq}[k] + u_{sw}[k]
$$

**Parameter choices**:
- $K_{sw}$: must exceed the worst-case disturbance. Because $u_{sw}$ is obtained by dividing by
  $B_d^i$, the gain carries the same unit as the sliding surface (A). Sizing it so the switching
  term commands at most 30% of the maximum phase voltage $V_{dc}/\sqrt{3}$ gives the starting point:
  $K_{sw} = 0.3 \cdot B_d^i \cdot V_{dc} / \sqrt{3}$.
- $\phi$: boundary layer width (A). Typical: $0.1$–$0.5$ A. Smaller gives tighter tracking but
  more high-frequency actuation.

**Discrete stability constraint** — this bounds both parameters together and is not optional. Inside
the boundary layer $\mathrm{sat}(e/\phi) = e/\phi$, so the equivalent term cancels the plant pole
and the closed-loop error obeys

$$
e[k+1] = -\frac{K_{sw}}{\phi}\, e[k]
$$

The error therefore contracts **only if $K_{sw} < \phi$**. A ratio at or above unity makes the
discrete loop diverge no matter how the equivalent term is computed, so the sizing rule above must be
capped by this constraint. The shipped defaults are $K_{sw} = 0.2$ A and $\phi = 0.5$ A, a ratio of
$0.4$.

**Robustness**: Insensitive to $R_s/L_s$ mismatch as long as mismatch is bounded by $K_{sw}$.
Covers $\pm 50\%$ thermal variation in $R_s$ and the initial RLS convergence transient.

**Relation to toolbox**: `SlidingModeControl<float,1,1>` implements equivalent + switching with
boundary-layer saturation on the error alone; the equilibrium term $R_s i^*$ is added by
`SlidingModeCurrentController`. Plant matrices $A_d^i, B_d^i$ are constructed from RLS estimates.

---

## Output Voltage Limit

The output voltage vector is subject to the circular SVM constraint:

$$
\sqrt{(v_d')^2 + (v_q')^2} \leq 1
$$

See `documentation/theory/current-loop-pi.md` — *Output Voltage Limit* — for the full derivation.
A3 has no integral action, so it is unaffected by windup at the voltage limit.

---

## Numerical Properties

| Property                 | Value                                             |
|--------------------------|---------------------------------------------------|
| ISR cost                 | ~12 MACs (equivalent + switching, 2 axes)         |
| Settling time            | ~$1/\omega_{bw}$ (boundary-layer limited)         |
| Robustness to Rs/Ls error| High — gain-bounded; covers ±50% $R_s$ drift      |
| Requires $\psi_f$        | No                                                |
| Requires RLS convergence | Partial — stable with datasheet values            |
| Tuning knobs             | 2 ($K_{sw}$, $\phi$)                              |

---

## Limitations & Assumptions

- Boundary layer $\phi$ introduces a steady-state band proportional to $\phi / K_{sw}$. Set $\phi$
  small enough that this band is below ADC resolution.
- $K_{sw}$ must exceed the worst-case disturbance; undersizing prevents reaching the sliding surface.
- **Stability constraint**: $K_{sw} < \phi$ is mandatory for discrete convergence.
- Assumes balanced three-phase operation ($i_a + i_b + i_c = 0$).
- Requires accurate electrical angle $\theta_e$. Angle errors couple directly into the dq frame.
- Output subject to the circular SVM constraint — see *Output Voltage Limit* above.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Utkin, V. — *Sliding Modes in Control and Optimization*, Springer, 1992.
   (Equivalent control; boundary-layer chattering suppression.)
3. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
