---
title: "Speed Loop — PI (Baseline)"
type: theory
status: draft
version: 1.0.0
component: "speed-loop-pi"
date: 2026-08-31
---

| Field     | Value                      |
|-----------|----------------------------|
| Title     | Speed Loop — PI (Baseline) |
| Type      | theory                     |
| Status    | draft                      |
| Version   | 1.0.0                      |
| Component | speed-loop-pi              |
| Date      | 2026-08-31                 |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

The baseline speed controller is a discrete incremental PI regulator that outputs a $q$-axis current
reference $i_q^*$ for the inner current loop. Pole-zero cancellation against the mechanical speed
plant places the closed-loop speed bandwidth at $\omega_{bw}^s$ directly. No mechanical RLS
convergence is required — datasheet values for $J$ and $B_f$ are sufficient, and the integral term
corrects steady-state error from model mismatch.

Operates exclusively in the **1 kHz outer handler**.

---

## Prerequisites

| Symbol        | Meaning                                   | Unit      |
|---------------|-------------------------------------------|-----------|
| $J$           | Rotor moment of inertia                   | kg·m²     |
| $B_f$         | Viscous friction coefficient              | N·m·s/rad |
| $K_t$         | Torque constant $= \tfrac{3}{2} p \psi_f$ | N·m/A     |
| $T_s^o$       | Outer loop sample period ($= 1$ ms)       | s         |
| $\omega_{bw}$ | Desired closed-loop speed bandwidth       | rad/s     |
| $I_{q,max}$   | Maximum $q$-axis current (hardware limit) | A         |

See `documentation/theory/foc-plant-models.md` for all base symbols and ZOH derivations.

---

## Mathematical Foundation

### Plant Model

Treating the current loop as ideal ($i_q \approx i_q^*$), the mechanical speed dynamics are:

$$
J \frac{d\omega_m}{dt} = K_t\, i_q^* - B_f\, \omega_m - T_L
$$

This is a first-order plant with a single real pole. The continuous-time transfer function from
$i_q^*$ to $\omega_m$ (ignoring load disturbance $T_L$) is:

$$
G_\omega(s) = \frac{K_t}{J s + B_f} = \frac{K_t/J}{s + B_f/J}
$$

ZOH-discretized at $T_s^o$ (from `documentation/theory/foc-plant-models.md` §2):

$$
\omega_m[k+1] = A_d^o\, \omega_m[k] + B_d^o\, u[k], \qquad
A_d^o = e^{-B_f T_s^o / J},\quad B_d^o = \frac{K_t T_s^o}{J}
$$

### PI Gain Design — Pole-Zero Cancellation

The mechanical plant has a single pole at $s = -B_f/J$. A PI controller:

$$
C(s) = K_p \cdot \frac{s + K_i/K_p}{s}
$$

achieves pole-zero cancellation by placing the PI zero at the plant pole:

$$
\frac{K_i}{K_p} = \frac{B_f}{J}
$$

The open-loop gain is then $K_p K_t / (Js)$, giving a first-order closed loop with bandwidth
$\omega_{bw}$. Solving for gains:

$$
\boxed{K_p = \frac{J\, \omega_{bw}}{K_t}, \qquad K_i = \frac{B_f\, \omega_{bw}}{K_t}}
$$

**Practical limit**: speed bandwidth must satisfy the cascade separation principle:
$\omega_{bw}^s \leq \omega_{bw}^i / 10$, where $\omega_{bw}^i$ is the current-loop bandwidth.
Typical: $\omega_{bw}^s = 100$–$500$ rad/s at a 1 kHz control rate.

### Incremental Form and Anti-Windup

The implementation uses the incremental PI to avoid initialisation issues:

$$
\Delta u[k] = K_p\,\bigl(e[k] - e[k-1]\bigr) + K_i\, T_s^o\, e[k]
$$
$$
u[k] = \mathrm{clamp}\bigl(u[k-1] + \Delta u[k],\ -I_{q,max},\ I_{q,max}\bigr)
$$

where $e[k] = \omega_m^*[k] - \omega_m[k]$. Clamping to $\pm I_{q,max}$ limits the current
demand to the hardware envelope and provides inherent anti-windup — no separate back-calculation
is needed.

---

## Numerical Properties

| Property                   | Value                                |
|----------------------------|--------------------------------------|
| Ops per 1 kHz cycle        | ~6 MACs                              |
| Steady-state speed error   | Zero (integral action)               |
| Load disturbance rejection | Integral quality — bandwidth-limited |
| Tracking vs. stiffness     | Coupled (single DOF)                 |
| Requires mechanical RLS    | No — datasheet $J$, $B_f$ sufficient |
| Tuning knobs               | 1 ($\omega_{bw}$)                    |

---

## Limitations & Assumptions

- Treats the current loop as ideal ($i_q \approx i_q^*$). Valid when the current-loop bandwidth
  is at least 10× the speed bandwidth.
- Pole-zero cancellation accuracy depends on knowledge of $B_f/J$. Model mismatch shifts the
  cancellation but integral action removes any resulting steady-state error.
- Single-DOF structure — cannot independently shape command tracking and disturbance rejection.
  Use S3 (Two-DOF) when both are required simultaneously.
- Speed estimate must be smooth. A noisy encoder derivative degrades this controller equally to
  all speed controllers; an alpha-beta filter or Kalman smoother is recommended upstream.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
3. Åström, K.J. & Hägglund, T. — *Advanced PID Control*, ISA, 2006.
