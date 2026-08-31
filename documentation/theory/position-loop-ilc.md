---
title: "Position Loop — P4: Iterative Learning Control (ILC)"
type: theory
status: draft
version: 1.0.0
component: "position-loop-ilc"
date: 2026-08-31
---

| Field     | Value                                          |
|-----------|------------------------------------------------|
| Title     | Position Loop — P4: Iterative Learning Control |
| Type      | theory                                         |
| Status    | draft                                          |
| Version   | 1.0.0                                          |
| Component | position-loop-ilc                              |
| Date      | 2026-08-31                                     |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

P4 (ILC — Iterative Learning Control) is for repetitive servo tasks — pick-and-place, CNC passes,
inspection cycles. The same position tracking error recurs on every trial because the same reference
is executed against the same mechanical dynamics. ILC exploits this: it learns the per-sample
feedforward correction that cancels the repeating error over successive trials until the residual is
near the sensor noise floor. No other controller achieves this tracking accuracy for periodic tasks.

ILC does not replace the feedback controller — it augments any stable position feedback controller
(P1, P2, or P3). Operates exclusively in the **1 kHz outer handler**.

---

## Prerequisites

| Symbol | Meaning                         | Unit |
|--------|---------------------------------|------|
| $Q$    | ILC robustness filter scalar    | —    |
| $\ell$ | ILC learning gain               | —    |
| $N$    | Trial length in samples (fixed) | —    |

See `documentation/theory/foc-plant-models.md` §3 for the position plant derivation.

---

## Mathematical Foundation

All position-loop controllers operate on the **discrete two-state position plant** from
`documentation/theory/foc-plant-models.md` §3. P4 augments any stable feedback controller with a
per-sample learned feedforward. Friction compensation adds a nonlinear Iq correction outside the
linear plant model.

---

## P4 — Iterative Learning Control (ILC)

**D-type update law**: Let $e^{(j)}[k] = \theta_m^*[k] - \theta_m^{(j)}[k]$ be the position error
on trial $j$ at sample $k$:

$$
\boxed{u_{ILC}^{(j+1)}[k] = Q \cdot \Bigl(u_{ILC}^{(j)}[k] + \ell \cdot e^{(j)}[k+1]\Bigr)}
$$

where:
- $Q \in (0,\,1]$: robustness filter. $Q < 1$ suppresses high-frequency noise at the cost of slower
  convergence. Typical: $Q = 0.95$.
- $\ell > 0$: learning gain. Convergence guaranteed for $\ell \in (0,\,2)$ on a unity-gain plant.

**Total control signal**: Learned correction added as feedforward to any position feedback controller:

$$
u_{total}^{(j)}[k] = u_{fb}^{(j)}[k] + u_{ILC}^{(j)}[k]
$$

$u_{fb}$ is the output of the active position feedback controller (P2, P1, or P3). ILC does not
replace the feedback controller — it augments it.

**Storage**: One full trial of $N$ samples stored in a bounded array. At 1 kHz, a 5-second trial
requires $N = 5000$ floats (20 kB). $N$ is fixed at build time; ILC is the only position algorithm
whose storage scales with trial duration.

**Convergence condition**: $|Q(1 - \ell\,P_{nom})| < 1$ across all frequencies, where $P_{nom}$
is the nominal position plant. Guaranteed by $Q < 1$ and $\ell$ within its stability range.

**Tuning**:
- $N$: trial length in samples (must exactly match the repetitive task period).
- $Q$: robustness vs. convergence speed tradeoff.
- $\ell$: learning rate. Start at $0.5$, increase toward $1.0$.

---

## Numerical Properties

| Property                    | Value                                         |
|-----------------------------|-----------------------------------------------|
| Ops per 1 kHz cycle         | 2 MACs + 1 array read                         |
| Steady-state position error | Near-zero after learning cycles               |
| Storage                     | $N \times$ `float` (bounded array, fixed $N$) |
| Suitable for                | Periodic / repetitive references only         |
| Requires J, Bf              | No                                            |
| Tuning knobs                | 3 ($N$, $Q$, $\ell$)                          |

---

## Limitations & Assumptions

- Only valid for strictly repetitive references. A changed reference period or non-repeatable
  reference (jogging) will cause the learned correction from the previous trial to degrade tracking.
- Trial length $N$ is fixed at selection time. It cannot change at runtime.
- ILC requires a stable inner feedback controller (P1, P2, or P3). ILC alone does not stabilise
  position — it only improves tracking of the feedback loop.
- Encoder noise enters position and velocity estimates. A state observer upstream improves robustness.

---

## References

1. Bristow, D.A., Tharayil, M. & Alleyne, A.G. — "A Survey of Iterative Learning Control",
   *IEEE Control Systems Magazine*, 26(3):96–114, 2006.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
