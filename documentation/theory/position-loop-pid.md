---
title: "Position Loop — P (Baseline)"
type: theory
status: draft
version: 1.0.0
component: "position-loop-pid"
date: 2026-08-31
---

| Field     | Value                        |
|-----------|------------------------------|
| Title     | Position Loop — P (Baseline) |
| Type      | theory                       |
| Status    | draft                        |
| Version   | 1.0.0                        |
| Component | position-loop-pid            |
| Date      | 2026-08-31                   |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

The baseline position controller converts position error into a speed reference $\omega_m^*$ for
the inner speed loop. Under cascade separation — inner speed loop bandwidth at least 5× the position
bandwidth — the closed-loop position bandwidth equals $K_p$ directly. No parameter identification is
required.

The law is a **PI**, not the pure P this chapter derives. The derivation below is the P design the
cascade shipped with and still the way to read the proportional path; the integral term was added on
top of it, weighted by the ratio of the integral weight to the position-error weight, and removes
the standing error the P design leaves under a constant velocity reference. When the integral weight
is zero the law reduces exactly to the P controller derived here.

Operates exclusively in the **1 kHz outer handler**.

---

## Prerequisites

| Symbol           | Meaning                                              | Unit  |
|------------------|------------------------------------------------------|-------|
| $\omega_{bw}^p$  | Desired closed-loop position bandwidth               | rad/s |
| $\omega_{bw}^s$  | Inner speed-loop closed-loop bandwidth               | rad/s |
| $\omega_{m,max}$ | Maximum mechanical angular velocity (hardware limit) | rad/s |
| $T_s^o$          | Outer loop sample period ($= 1$ ms)                  | s     |

See `documentation/theory/foc-plant-models.md` §3 for the position plant derivation.

---

## Mathematical Foundation

### Plant Model

Under cascade separation (treating the speed loop as ideal, $\omega_m \approx \omega_m^*$), the
position dynamics reduce to a pure integrator:

$$
\frac{d\theta_m}{dt} = \omega_m
$$

Transfer function from $\omega_m^*$ to $\theta_m$:

$$
G_\theta(s) = \frac{1}{s}
$$

### P Controller Design

A proportional controller $C(s) = K_p$ closes the integrator loop. This is the proportional path of
the implemented PI; see the note in the Overview:

$$
\boxed{\omega_m^*[k] = K_p \cdot \mathrm{wrap}\bigl(\theta_m^*[k] - \theta_m[k]\bigr)}
$$

The closed-loop transfer function is:

$$
\frac{\Theta_m(s)}{\Theta_m^*(s)} = \frac{K_p}{s + K_p}
$$

The position bandwidth equals the proportional gain directly:

$$
\omega_{bw}^p = K_p
$$

**Bandwidth selection**: Cascade separation requires:

$$
K_p = \omega_{bw}^p \leq \frac{\omega_{bw}^s}{5}
$$

Exceeding this limit causes the position loop to drive the speed loop into instability.

### Angle-Error Wrapping

Position error must be wrapped to $(-\pi, \pi]$ to ensure the shortest arc is always taken:

$$
e_\theta[k] = \mathrm{wrap}\bigl(\theta_m^*[k] - \theta_m[k]\bigr)
$$

$$
\mathrm{wrap}(x) = x - 2\pi\, \mathrm{round}\!\left(\frac{x}{2\pi}\right)
$$

Without wrapping, a $+3.1$ rad reference reached from $-3.1$ rad would command a $6.2$ rad
(354°) rotation through the long arc instead of the $0.08$ rad short arc.

### Output Limiting

The speed reference is clamped to $\pm\omega_{m,max}$ to prevent demanding speeds beyond the
mechanical or electrical limits of the drive:

$$
\omega_m^*[k] = \mathrm{clamp}\bigl(K_p \cdot e_\theta[k],\ -\omega_{m,max},\ \omega_{m,max}\bigr)
$$

---

## Numerical Properties

| Property                    | Value                                         |
|-----------------------------|-----------------------------------------------|
| Ops per 1 kHz cycle         | 2 MACs (1 multiply + wrap + clamp)            |
| Steady-state position error | Non-zero under constant velocity reference for the P design; removed by the integral term |
| Steady-state error at rest  | Zero (no steady velocity → no position error) |
| Requires J, Bf              | No                                            |
| Tuning knobs                | 2 ($K_p = \omega_{bw}^p$, and the integral weight relative to the position-error weight)  |

---

## Limitations & Assumptions

- Treats the speed loop as ideal. Valid when $\omega_{bw}^s \geq 5\, \omega_{bw}^p$.
- Under a constant velocity reference $\dot{\theta}_m^* \neq 0$, a pure P controller has
  non-zero steady-state tracking error. The implementation carries an integral term for this, and
  velocity feedforward (as in P2 / Cascade P) addresses it without the integral's phase cost.
- No disturbance rejection beyond what the speed loop provides.
- Encoder noise enters the position estimate. A state observer upstream improves robustness.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
