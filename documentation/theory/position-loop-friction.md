---
title: "Position Loop — Friction Compensation Feedforward"
type: theory
status: draft
version: 1.0.0
component: "position-loop-friction"
date: 2026-08-31
---

| Field     | Value                                             |
|-----------|---------------------------------------------------|
| Title     | Position Loop — Friction Compensation Feedforward |
| Type      | theory                                            |
| Status    | draft                                             |
| Version   | 1.0.0                                             |
| Component | position-loop-friction                            |
| Date      | 2026-08-31                                        |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

Friction compensation is not a selectable algorithm — it is an independently enabled feedforward
that stacks on top of any speed or position feedback controller. Viscous friction ($B_f \cdot \omega_m$)
is estimated by the mechanical RLS and compensated implicitly by the feedback controllers. Coulomb
and Stribeck friction are nonlinear: at low speeds the motor sticks until the drive overcomes the
static breakaway torque, then slips. This creates position hunting at rest and velocity ripple
during slow moves — the classic precision servo failure mode that no linear feedback controller can
fully eliminate.

---

## Prerequisites

| Symbol          | Meaning                           | Unit  |
|-----------------|-----------------------------------|-------|
| $T_c$           | Coulomb (kinetic) friction torque | N·m   |
| $T_s$           | Static (breakaway) torque         | N·m   |
| $\omega_{st}$   | Stribeck velocity                 | rad/s |
| $K_t$           | Torque constant                   | N·m/A |
| $\omega_{dead}$ | Dead-zone smoothing width         | rad/s |

Parameters $T_c$, $T_s$, $\omega_{st}$ are identified by a dedicated friction sweep — separate from
the mechanical RLS that estimates only the linear viscous coefficient $B_f$.

---

## Mathematical Foundation

### Friction Model (Coulomb + Stribeck)

$$
\boxed{T_f(\omega_m) = \Bigl[T_c + (T_s - T_c)\,e^{-(\omega_m/\omega_{st})^2}\Bigr]\cdot\mathrm{sgn}(\omega_m)}
$$

where:
- $T_c$: Coulomb (kinetic) friction torque — the flat plateau at moderate speed
- $T_s$: static (breakaway) torque, $T_s > T_c$ — the peak at near-zero speed
- $\omega_{st}$: Stribeck velocity — speed at which kinetic friction reaches its minimum

### Current Feedforward

Converting friction torque to $i_q$ and injecting before the current loop:

$$
i_{ff}(\omega_m) = \frac{T_f(\omega_m)}{K_t}
$$
$$
i_q^{*,total} = i_q^{*,ctrl} + i_{ff}(\omega_m)
$$

### Dead-Zone Smoothing

The $\mathrm{sgn}(\omega_m)$ discontinuity at $\omega_m = 0$ must be replaced with a linear ramp
over $\pm\omega_{dead}$ to prevent chattering during direction reversals and holding. Typical
$\omega_{dead} = 0.05$–$0.2$ rad/s.

### Parameter Identification

$T_c$, $T_s$, and $\omega_{st}$ require a dedicated friction sweep (ramp velocity at steady state
while logging $i_q^*$). This is separate from the mechanical RLS, which estimates only the linear
viscous coefficient $B_f$.

**Tuning**:
- $T_c$ (N·m): read from the $i_q^* \times K_t$ plateau at constant low speed.
- $T_s$ (N·m): read from the breakaway $i_q^*$ at near-zero speed.
- $\omega_{st}$ (rad/s): speed at which the $i_q^*$ vs. $\omega_m$ curve reaches its kinetic minimum.

---

## Numerical Properties

| Property       | Value                                          |
|----------------|------------------------------------------------|
| Applied to     | $i_q^*$ at speed or position controller output |
| Ops per cycle  | ~6 MACs + exp (fast approximation)             |
| Storage        | Constant (3 parameters)                        |
| Identification | Dedicated friction sweep required              |

---

## Limitations & Assumptions

- Over-compensation ($T_s$ or $T_c$ over-estimated) causes position overshoot at zero crossing.
  Prefer slight under-compensation and let the feedback controller handle the residual.
- Friction parameters drift with temperature and wear. Re-identification is recommended when
  low-speed precision degrades.
- The $\mathrm{sgn}(\omega_m)$ dead-zone must be smoothed to prevent chattering; too wide a
  dead-zone reintroduces hunting in the smoothed region.

---

## References

1. Armstrong-Hélouvry, B., Dupont, P. & De Wit, C.C. — "A Survey of Models, Analysis Tools
   and Compensation Methods for the Control of Machines with Friction",
   *Automatica*, 30(7):1083–1138, 1994.
2. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
