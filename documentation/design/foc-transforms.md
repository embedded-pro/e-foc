---
title: "FOC Mathematical Transforms"
type: design
status: draft
version: 0.1.0
component: foc-transforms
date: 2026-04-07
---

| Field     | Value                       |
|-----------|-----------------------------|
| Title     | FOC Mathematical Transforms |
| Type      | design                      |
| Status    | draft                       |
| Version   | 0.1.0                       |
| Component | foc-transforms              |
| Date      | 2026-04-07                  |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
> Prose descriptions of algorithms are encouraged; source-level details are not.
>
> **Diagrams**: All visuals must be either a Mermaid fenced code block (` ```mermaid `) or ASCII art inline
> in the document. External image references using Markdown image syntax are **not allowed**.

---

## Responsibilities

**Is responsible for:**
- Converting three-phase stator currents to a two-phase stationary frame (Clarke transform)
- Converting between the stationary αβ frame and the rotor-synchronous dq frame (Park transform)
- Providing a composite Clarke → Park (and inverse) operation that evaluates trigonometric functions exactly once per call
- Converting a two-component voltage vector in the αβ frame to three-phase PWM duty cycles (Space Vector Modulation)
- Supplying fast sine and cosine approximations via a pre-computed lookup table for use in the FOC hot path

**Is NOT responsible for:**
- Reading or writing hardware peripherals (ADC, PWM, encoder)
- Managing PID controllers or setpoints
- Determining rotor position — electrical angle must be supplied by the caller
- Any control strategy decisions (torque, speed, or position setpoints)
- Scaling voltage vectors to physical units — inputs and outputs are normalised fractions relative to the DC bus

---

## Component Details

### Clarke Transform — Three-Phase to Stationary αβ Frame

The Clarke transform is a geometric projection that reduces a balanced three-phase system to an equivalent two-axis representation. It requires no knowledge of rotor position.

**Forward transform** (currents 3-phase → αβ):

The amplitude-invariant Clarke transform uses all three measured phases, so it stays correct when
the currents are not perfectly balanced:

```text
Iα = (2/3)·(Ia - (Ib + Ic)/2)
Iβ = (Ib - Ic) / √3
```

When the system *is* balanced (`Ia + Ib + Ic = 0`) this reduces to the familiar two-sensor form
`Iα = Ia`, `Iβ = (Ia + 2·Ib)/√3`. The implementation uses the three-phase form unconditionally.

All three phase currents are measured. `AdcPhaseCurrentMeasurement` converts three ADC samples and
delivers them as a `PhaseCurrents` triple, so no phase is reconstructed from the balance constraint.

**Inverse transform** (voltages αβ → 3-phase):

Reconstructs the three-phase voltage references from Vα and Vβ, using the same geometric relationships in reverse. The output of the inverse Clarke is fed into the Space Vector Modulator.

**Invariants:**
- No balance assumption is required on the input; a nonzero common-mode current appears only in the
  zero-sequence component, which the two-axis output discards.
- The transform is purely linear and involves no state — every call is independent.

### Park Transform — Stationary αβ to Rotor-Synchronous dq Frame

The Park transform rotates the stationary αβ frame to align with the rotor magnetic field. The result is a coordinate frame that is stationary relative to the rotor, so DC steady-state values represent constant torque and flux components.

**Forward transform** (αβ → dq):

```text
Id =  Iα·cos(θe) + Iβ·sin(θe)
Iq = −Iα·sin(θe) + Iβ·cos(θe)
```

The d-axis (direct) current component is aligned with the rotor flux. For a surface-permanent-magnet synchronous motor (SPMSM), the d-axis setpoint is held at zero to maximise torque per ampere. The q-axis (quadrature) current is proportional to electromagnetic torque.

**Inverse transform** (dq → αβ):

```text
Vα = Vd·cos(θe) − Vq·sin(θe)
Vβ = Vd·sin(θe) + Vq·cos(θe)
```

The inverse Park transform is applied after the PID controllers produce voltage demands Vd and Vq, converting them back to the stationary frame for SVM processing.

**Electrical angle:**

The electrical angle θe is obtained by multiplying the mechanical rotor angle θm by the motor's pole-pair count P:

```text
θe = θm · P
```

The caller is responsible for supplying the correct θe.

**Invariants:**
- The same θe (and therefore the same cos/sin pair) must be used for both the forward Park in the current-sensing path and the inverse Park in the voltage-output path within a single control cycle.
- The transform is stateless — no memory of previous cycles.

### Composite ClarkePark — Chained Transform with Single Trigonometric Evaluation

The ClarkePark composite combines the forward Clarke and forward Park transforms (or their inverses) into a single operation. The motivation is efficiency: cos(θe) and sin(θe) are computed exactly once and reused for both the Park rotation in the forward direction and the inverse Park rotation.

The forward composite takes (Ia, Ib, θe) and produces (Id, Iq) directly.  
The inverse composite takes (Vd, Vq, θe) and produces (Vα, Vβ) directly.

This is the form used by all inner-control-loop implementations in this project.

### Space Vector Modulation — αβ Voltage Vector to PWM Duty Cycles

Space Vector Modulation (SVM) maps a two-component voltage reference vector in the αβ plane to three symmetrical PWM duty cycles. The implementation is *sector-free*: it reaches the same duty cycles as classical sector-and-dwell-time SVM by injecting a common-mode offset, which is algebraically equivalent and has no branch on sector index.

**Inverse Clarke:**

The reference is first expanded to three phase voltages, `vA = Vα`, `vB = -Vα/2 + (√3/2)·Vβ`, `vC = -Vα/2 - (√3/2)·Vβ`.

**Common-mode (min-max) injection:**

The offset `vCommon = -(max(vA,vB,vC) + min(vA,vB,vC)) / 2` is added to all three phases. Because it is
identical on every phase it cancels in every line-to-line voltage, so it changes no motor current while
centring the waveform. This is what recovers the full 2/√3 linear range that classical SVM obtains from
symmetric null-vector splitting.

**Duty cycle output:**

Each offset phase voltage is scaled by 1/√3, biased by 0.5 to centre it in the PWM period, and clamped:
`d = clamp(v/√3 + 0.5, 0, 1)`. A reference of unit magnitude therefore spans exactly [0, 1].

**Constraints:**
- Input Vα, Vβ are per-unit relative to `V_dc/√3` (dimensionless, inside the unit circle for linear operation).
- Output duty cycles are in the range [0.0, 1.0]; values outside this range are clamped, which is how
  over-modulation is handled.
- Being branch-free in the sector, the output is continuous across every sector boundary by construction.
- SVM does not accept angles directly — it operates only on the (Vα, Vβ) pair.

```mermaid
graph LR
    Va["Vα, Vβ (per-unit)"] --> Inv["Inverse Clarke\n(vA, vB, vC)"]
    Inv --> Cm["Common-mode injection\n-(max+min)/2"]
    Cm --> Duty["Scale, bias, clamp\n[0.0, 1.0]"]
```

### Fast Trigonometry — Lookup-Table Sine and Cosine

Direct hardware transcendental functions (`sin`, `cos`) introduce variable latency and are unsuitable for a 20 kHz ISR. A 512-entry pre-computed lookup table (LUT) covering one full period [0, 2π) provides both sine and cosine with bounded, cycle-predictable evaluation time.

**LUT characteristics:**
- 512 entries, each a 32-bit floating-point value
- Stored in ROM (read-only, `constexpr`) at a 16-byte memory alignment boundary
- Total ROM footprint: 512 × 4 bytes = 2 048 bytes (2 KB)
- Covers exactly one full period; the angle is normalised to [0, 2π) before indexing

**Cosine** is derived from the sine LUT by a quarter-period index offset — no separate cosine table is required.

**Interpolation:** Linear interpolation between adjacent LUT entries provides accuracy sufficient for FOC applications; the quantisation error is below the noise floor of 12-bit ADC current sensing.

---

## Interfaces

### Provided

| Interface                        | Purpose                                       | Contract                                                                                                                                       |
|----------------------------------|-----------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|
| Clarke — Forward                 | Converts (Ia, Ib) to (Iα, Iβ)                 | Ic is derived internally; inputs must satisfy the balanced-phase assumption. Output is immediately valid.                                      |
| Clarke — Inverse                 | Converts (Vα, Vβ) to (Va, Vb, Vc)             | Produces all three phase voltages. The sum of outputs is zero.                                                                                 |
| Park — Forward                   | Converts (Iα, Iβ, θe) to (Id, Iq)             | Caller supplies pre-computed or LUT-evaluated cos(θe) and sin(θe). Stateless.                                                                  |
| Park — Inverse                   | Converts (Vd, Vq, θe) to (Vα, Vβ)             | Uses the same θe as the forward Park in the same control cycle.                                                                                |
| ClarkePark — Forward             | Converts (Ia, Ib, θe) to (Id, Iq) in one call | Computes cos/sin once; result is identical to chaining Clarke then Park separately.                                                            |
| ClarkePark — Inverse             | Converts (Vd, Vq, θe) to (Vα, Vβ) in one call | Computes cos/sin once; result is identical to chaining inverse Park then inverse Clarke separately.                                            |
| SpaceVectorModulation — Generate | Converts (Vα, Vβ) to three duty cycles        | Inputs must be per-unit relative to `V_dc/√3`. Outputs are always in [0.0, 1.0]. Common-mode injection is internal; there is no sector branch. |
| FastTrigonometry — Sine          | Returns an approximation of sin(θ)            | θ is normalised to [0, 2π) internally. ROM LUT; no floating-point transcendental at runtime.                                                   |
| FastTrigonometry — Cosine        | Returns an approximation of cos(θ)            | Derived from the sine LUT via a quarter-period offset. Same ROM, no additional storage.                                                        |

### Required

| Interface | Purpose                                                                        | Contract |
|-----------|--------------------------------------------------------------------------------|----------|
| None      | All transforms are pure mathematical algorithms with no external dependencies. | —        |

---

## Data Model

| Entity              | Field      | Type / Unit           | Range           | Notes                                           |
|---------------------|------------|-----------------------|-----------------|-------------------------------------------------|
| Three-phase current | Ia, Ib     | Ampere (float)        | ± rated current | Ic derived; not stored                          |
| Stationary frame    | Iα, Iβ     | Ampere (float)        | ± rated current | Output of Clarke forward                        |
| Synchronous frame   | Id, Iq     | Ampere (float)        | ± rated current | Output of Park forward                          |
| Voltage demand      | Vd, Vq     | Dimensionless (float) | [−1, +1]        | Normalised to DC bus                            |
| Stationary voltage  | Vα, Vβ     | Dimensionless (float) | [−1, +1]        | Output of inverse Park / input to SVM           |
| Phase duty cycles   | Da, Db, Dc | Dimensionless (float) | [0.0, 1.0]      | Clamped; 0 = always low, 1 = always high        |
| Electrical angle    | θe         | Radians (float)       | [0, 2π)         | θm × pole_pairs; normalised before LUT indexing |
| LUT                 | Sine table | float[512]            | [−1.0, +1.0]    | constexpr; 2 KB ROM; 16-byte aligned            |

---

## Block Diagram

```mermaid
graph LR
    Ia["Ia, Ib (Ampere)"] --> Clarke["Clarke\nForward"]
    Clarke --> Ialpha["Iα, Iβ"]
    Ialpha --> Park["Park\nForward"]
    Theta["θe (Radians)"] --> Trig["FastTrigonometry\nLUT"]
    Trig --> Park
    Trig --> InvPark["Park\nInverse"]
    Park --> dq["Id, Iq"]
    dq -->|PID controllers upstream| Vdq["Vd, Vq"]
    Vdq --> InvPark
    InvPark --> Vab["Vα, Vβ"]
    Vab --> SVM["Space Vector\nModulation"]
    SVM --> Duty["Da, Db, Dc\n[0.0, 1.0]"]
```

---

## Constraints & Limitations

| Constraint                      | Value / Description                                                                                  |
|---------------------------------|------------------------------------------------------------------------------------------------------|
| Two-sensor current topology     | Ic is derived, not measured. Clarke forward is invalid if the three-phase system is unbalanced.      |
| LUT ROM footprint               | 2 048 bytes (2 KB) of read-only memory.                                                              |
| LUT accuracy                    | Approximation error is bounded and below the noise floor of 12-bit ADC current sensing.              |
| SVM input normalisation         | Vα, Vβ must be dimensionless fractions in [−1, +1] relative to the DC bus, not physical volt values. |
| SVM output saturation           | Duty cycles are clamped to [0.0, 1.0]. Over-modulation is silently saturated, not flagged.           |
| Electrical angle responsibility | The caller is responsible for computing θe = θm × pole_pairs before invoking any Park operation.     |
| Stateless transforms            | No transform retains state between calls. Every call is independent.                                 |
| Hot-path constraint             | All transforms must execute within the cycle budget of the 20 kHz FOC interrupt.                     |
