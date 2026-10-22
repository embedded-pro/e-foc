---
title: "Position Loop — P (Baseline)"
type: theory
status: draft
version: 1.2.0
component: "position-loop-pid"
date: 2026-10-22
---

| Field     | Value                        |
|-----------|------------------------------|
| Title     | Position Loop — P (Baseline) |
| Type      | theory                       |
| Status    | draft                        |
| Version   | 1.2.0                        |
| Component | position-loop-pid            |
| Date      | 2026-10-22                   |

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
cascade shipped with and still the way to read the proportional path; the integral term removes the
standing error a speed disturbance leaves while the rotor moves. Its zero and the weight the
proportional term gives the reference are fixed by the design in *Integral zero and reference
weighting* below, not by the loop tunings, and a rotor that stands still is treated as *A rotor that
stands still* describes.

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

### Integral zero and reference weighting

The integral zero sits at a fifth of the position bandwidth, $K_i = 0.2\,\omega_{bw}^p K_p$, and the
proportional term acts on $b\,\theta^* - \theta$ with $b = 0.75$ rather than on the error:

$$
\omega_m^*[k] = \omega_m^*[k-1] + K_p\left(b\,\Delta\theta^*[k] - \Delta\theta_m[k]\right) + K_i T_s^o\, e_\theta[k]
$$

in the velocity form the implementation uses, with every difference wrapped as the error is.

The zero used to sit at the integral weight times the bandwidth, a twentieth of it with the default
weight. That places a slow closed-loop pole just beside the zero, and the pair leaves a tail whose
size is the residue of that pole: under an ideal speed loop at $\omega_{bw}^p = 18.85$ rad/s the
poles fall at $-0.995$ and $-17.9$ rad/s against a zero at $-0.94$, and a 1.5 rad step keeps some
0.089 rad of overshoot that decays over a second — 0.055 rad is still there half a second later. A
torque step pushing the same way as that tail starts its excursion already most of the way across a
0.08 rad recovery band.

Moving the zero up to $\omega_{bw}^p/5$ alone shortens the tail but makes it larger, because the zero
also lies in the reference path. Weighting the reference in the proportional term takes the zero out
of the reference path without touching disturbance rejection, which sees the full $K_p$ through
$\Delta\theta_m$. With $b = 0.75$ a 1.5 rad step overshoots by well under 1 % and is within 0.01 rad
of the setpoint 450 ms after the step.

The weight has a price under a moving setpoint. A constant velocity reference $\dot{\theta}^*$ leaves
the proportional term $(1 - b)\,K_p\,\dot{\theta}^* T_s^o$ short every period, and the integral can
only make that up from a standing error, $e_\theta = (1 - b)\,\dot{\theta}^* / (0.2\,\omega_{bw}^p)$:
0.13 rad at 2 rad/s, a quarter more than the P design's $\dot{\theta}^* / \omega_{bw}^p$.

### A rotor that stands still

While the measured speed is under 2 % of the speed envelope, about 1.2 rad/s at the default bandwidth
and less than one count per period on a 4000-count encoder, the integral step is replaced by a
relaxation of the command towards the speed the error alone calls for, at the integral's own rate:

$$
\omega_m^*[k] = \omega_m^*[k-1] + K_p\left(b\,\Delta\theta^*[k] - \Delta\theta_m[k]\right) + \alpha\left(K_p\, e_\theta[k] - \omega_m^*[k-1]\right), \qquad \alpha = 0.2\,\omega_{bw}^p T_s^o
$$

From a zero command the relaxation is exactly the integral step, so a step from rest starts as before.
What it removes is the integral's memory while nothing moves. The weighted proportional term leaves
$(1 - b)\,K_p$ of every setpoint change for the integral to supply, and on a smooth shaft the
integral finishes that while the rotor approaches. A detent or dry friction that stops the rotor near
the setpoint used to leave it unfinished, or winding: the command grew while the rotor stood still,
the speed loop's integral raised the torque until the rotor broke away, the excess carried it past the
setpoint, and the cycle repeated. On the bench, 24 detents a revolution about 4 mN·m deep and
3.7 mN·m of dry friction made it a limit cycle of ±0.17 rad every 1.5 s at 1 rad.

Relaxed, the command settles at $K_p e_\theta$, the speed loop's integral supplies whatever torque
holds the rotor, and the rotor creeps onto the setpoint as it does under Cascade P. Nothing is lost at
rest: every speed law rejects a constant torque with no speed error, so no disturbance needs the
position integral to hold a stationary rotor.

### Output Limiting

The speed reference is clamped to $\pm\omega_{m,max}$ to prevent demanding speeds beyond the
mechanical or electrical limits of the drive:

$$
\omega_m^*[k] = \mathrm{clamp}\bigl(K_p \cdot e_\theta[k],\ -\omega_{m,max},\ \omega_{m,max}\bigr)
$$

---

## Numerical Properties

| Property                    | Value                                                                                  |
|-----------------------------|----------------------------------------------------------------------------------------|
| Ops per 1 kHz cycle         | 2 MACs (1 multiply + wrap + clamp)                                                     |
| Steady-state position error | $(1 - b)\,\dot{\theta}^* / (0.2\,\omega_{bw}^p)$ under a constant velocity reference   |
| Steady-state error at rest  | Zero (no steady velocity → no position error)                                          |
| Requires J, Bf              | No                                                                                     |
| Tuning knobs                | 1 ($K_p = \omega_{bw}^p$); the integral zero and reference weight are design constants |

---

## Limitations & Assumptions

- Treats the speed loop as ideal. Valid when $\omega_{bw}^s \geq 5\, \omega_{bw}^p$.
- Under a constant velocity reference $\dot{\theta}_m^* \neq 0$ the law keeps a standing tracking
  error (*Integral zero and reference weighting*). Velocity feedforward (as in P2 / Cascade P)
  addresses it without the integral's phase cost.
- A setpoint between two cogging detents sits where the cogging torque pushes the rotor away from it.
  The loop holds there only while its stiffness, $2 J\,\omega_{bw}^s\,\omega_{bw}^p$, exceeds the
  cogging's, the detent count times its depth. On the bench at the default bandwidths the two are
  about equal, 0.10 against 0.096 N·m/rad, and the rotor steps between the neighbouring detents
  instead; 3.0 rad is such a setpoint there. Cascade P does the same. The calibration's cogging
  map removes the cogging from what the loop has to hold (`service-cogging-compensation.md`), and
  with it every position law holds 3.0 rad (REQ-POS-012).
- No disturbance rejection beyond what the speed loop provides.
- Encoder noise enters the position estimate. A state observer upstream improves robustness.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
