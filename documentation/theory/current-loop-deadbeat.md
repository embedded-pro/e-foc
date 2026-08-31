---
title: "Current Loop — A2: Deadbeat"
type: theory
status: draft
version: 1.0.0
component: "current-loop-deadbeat"
date: 2026-08-31
---

| Field     | Value                       |
|-----------|-----------------------------|
| Title     | Current Loop — A2: Deadbeat |
| Type      | theory                      |
| Status    | draft                       |
| Version   | 1.0.0                       |
| Component | current-loop-deadbeat       |
| Date      | 2026-08-31                  |

> **Theory document**: Explains the mathematical and engineering principles behind a component or algorithm.
> This document is descriptive — it records the *why* and *how* at a scientific level, independent of any
> specific implementation. Equations use KaTeX ($inline$ and $$block$$). Block diagrams use Mermaid or
> ASCII art.

---

## Overview

A2 (Deadbeat) inverts the discrete current plant model to compute the exact voltage that drives
the current to its reference in one sampling step — the minimum physically achievable settling
time (50 µs at 20 kHz). This maximises servo stiffness: the torque response is instantaneous
relative to any outer loop, so load disturbances are rejected before position error can accumulate.
A two-step variant reduces noise amplification at the cost of geometric (non-deadbeat) settling.

Operates exclusively in the **20 kHz FOC ISR**. Requires tight electrical RLS convergence ($L_s$,
$R_s$) and the A1 feedforward for decoupling at speed.

---

## Prerequisites

| Symbol        | Meaning                                           | Unit       |
|---------------|---------------------------------------------------|------------|
| $A_d^i, B_d^i$| Discrete current plant matrices                   | —          |
| $i^*$         | Current reference (d or q axis)                   | A          |
| $i[k]$        | Measured current                                  | A          |
| $V_{dc}$      | DC bus voltage                                    | V          |

See `documentation/theory/foc-plant-models.md` for ZOH derivations of $A_d^i$, $B_d^i$.
See `documentation/theory/current-loop-decoupled-pid.md` for the A1 feedforward used alongside A2.

---

## Mathematical Foundation

All current controllers operate on the **decoupled per-axis RL plant** derived in
`documentation/theory/foc-plant-models.md` §1:

$$
i[k+1] = A_d^i \cdot i[k] + B_d^i \cdot v'[k]
$$

A2 inverts this plant model directly and therefore applies the **same A1 feedforward** to its
inversion result — an exact plant inversion is only exact once the coupling it does not model
has been cancelled.

---

## A2 — Deadbeat Current Control

**One-step control law**: Setting $i[k+1] = i^*$ and solving for $v'$:

$$
\boxed{v'[k] = \frac{i^* - A_d^i\, i[k]}{B_d^i}}
$$

The result is clamped to $[-1,\,1]$ (normalised). When the step demand exceeds available voltage,
settling takes more than one sample but remains optimal for the available headroom.

**Two-step variant** (noise robustness): The one-step law amplifies measurement noise by
$1/B_d^i = L_s/T_s^i$, which is large for small inductances. The two-step law solves for the
**minimum-norm input sequence** over a two-sample horizon and applies its first element, with the
reference gain corrected so that the closed loop retains unity DC gain:

$$
v'[k] = \frac{\left((A_d^i)^2 - A_d^i + 1\right) i^* - (A_d^i)^3\, i[k]}{B_d^i\left((A_d^i)^2 + 1\right)}
$$

This reduces the state gain — and hence noise amplification — by roughly half.

**Receding-horizon behaviour**: because only the first element of the sequence is applied and the
solution is recomputed every sample, the two-step law does *not* settle in exactly two samples. The
closed-loop pole is $A_d^i/\left((A_d^i)^2+1\right)$, giving a geometric rather than deadbeat
response. Tracking is nonetheless exact in steady state: the reference gain is obtained by solving

$$
B_d^i\, g_{ref} = 1 - A_d^i + B_d^i\, g_{state}
$$

which forces unity DC gain. Taking the raw minimum-norm reference gain instead leaves a static error
of $A_d^i/\left((A_d^i)^2 - A_d^i + 1\right)$ — around 1% at $A_d^i = 0.90$ and 20% at $0.61$ —
which is worst in the small-$L_s$ regime the variant targets.

The one-step law is unaffected — it places the closed-loop pole at the origin and tracks exactly.

**Model accuracy requirement**: A 10% $L_s$ error leaves a 10% residual error after one step.
Deadbeat is the natural progression after Decoupled PID has validated RLS convergence.

**Decoupling**: the inversion above solves the decoupled plant, so the A1 feedforward terms are
added to $v'$ before the modulation-circle limit. Without them $\omega_e L_s i_q$ enters as an
unmodelled disturbance and the one-step settling property is lost at speed.

**Normalisation**: $v'[k]$ (physical volts) normalised identically to the PI output:
$v'_{norm}[k] = v'[k] \cdot \sqrt{3}/V_{dc}$.

**Tuning**: None — fully determined by $A_d^i, B_d^i$ from RLS. Only design choice: one-step vs.
two-step variant.

---

## Output Voltage Limit

The output voltage vector is subject to the circular SVM constraint:

$$
\sqrt{(v_d')^2 + (v_q')^2} \leq 1
$$

See `documentation/theory/current-loop-pi.md` — *Output Voltage Limit* — for the full derivation.
Deadbeat has no integral action, so it is unaffected by windup at the voltage limit.

---

## Numerical Properties

| Property                 | Value                                             |
|--------------------------|---------------------------------------------------|
| ISR cost                 | ~4 MACs (cheapest advanced controller)            |
| Settling time            | 1 sample (one-step); geometric (two-step)         |
| Robustness to Rs/Ls error| Low — exact model required                        |
| Requires $\psi_f$        | No (but A1 feedforward does)                      |
| Requires RLS convergence | Yes — tight convergence required                  |
| Tuning knobs             | 0 (variant choice only)                           |

---

## Limitations & Assumptions

- Most sensitive to $L_s$ error of all current controllers. Do not activate until electrical RLS
  has converged (typically after the first few seconds of operation under load).
- The two-step variant halves noise amplification and still tracks exactly, but gives up deadbeat
  settling for a geometric response with pole $A_d^i/\left((A_d^i)^2+1\right)$. Prefer it for
  noisy current measurement or small $L_s$; prefer one-step for maximum servo stiffness.
- Assumes balanced three-phase operation ($i_a + i_b + i_c = 0$).
- Requires accurate electrical angle $\theta_e$. Angle errors couple directly into the dq frame.
- Output subject to the circular SVM constraint — see *Output Voltage Limit* above.

---

## References

1. Krishnan, R. — *Permanent Magnet Synchronous and Brushless DC Motor Drives*, CRC Press, 2010.
2. Åström, K.J. & Wittenmark, B. — *Computer-Controlled Systems*, 3rd ed., Prentice Hall, 1997.
