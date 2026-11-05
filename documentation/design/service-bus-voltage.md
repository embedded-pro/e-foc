---
title: "Service: Bus Voltage"
type: design
status: draft
version: 0.1.0
component: service-bus-voltage
date: 2026-11-05
---

| Field     | Value                |
|-----------|----------------------|
| Title     | Service: Bus Voltage |
| Type      | design               |
| Status    | draft                |
| Version   | 0.1.0                |
| Component | service-bus-voltage  |
| Date      | 2026-11-05           |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
> Prose descriptions of algorithms are encouraged; source-level details are not.
>
> **Diagrams**: All visuals must be Mermaid (` ```mermaid `), TikZ via ` ```{=latex} ` raw blocks (source in
> `documentation/tikz/`), or ASCII art. External image references (`![alt](path)`) are **not allowed**.

---

## Responsibilities

The motor model is configured once with the bus voltage read at boot, before the bridge switches. A bus
that sags under load or rises under regeneration then scales the voltage the winding gets. This service
measures the bus while the drive runs and hands the cascade a filtered value, so that the cascade can
scale its duties by the measured over the configured voltage (`foc-torque.md`, Bus Voltage Tracking).

**Is responsible for:**
- Reading the platform's bus voltage every 10 ms from the event loop
- Filtering the readings with a 50 ms time constant, starting from the first reading
- Handing every filtered value to the control mode, which passes it to the active cascade

**Is NOT responsible for:**
- Scaling the duties, which the cascade does
- Bus over- and under-voltage protection, which the board's comparators and the fault path handle
- The bus voltage the identification procedures use, which is still the one read at boot

---

## Component Details

### Part A — Reading the bus

The platform's bus voltage read blocks for its conversion: about 134 µs on the EK-TM4C1294XL, whose
divider is sampled for 256 ADC clocks with eight-fold oversampling. The service therefore reads it from
the event loop, never from an interrupt, once every 10 ms, which costs about 1.3 % of the event loop. The
control interrupt preempts the read, so the 20 kHz loop is untouched.

### Part B — Filtering

Each reading after the first moves the filtered value by a sixth of its distance to the reading: one
10 ms step of a first-order filter with a 50 ms time constant. The first reading is taken as it is, so the
cascade starts from the measured bus rather than ramping up from zero. A step of the bus is therefore
followed to within 5 % in about 150 ms.

### Part C — Handing it on

Every filtered value goes to the control mode, which keeps the latest one and passes it to the active
cascade, and passes it again to the cascade a control-mode switch builds. A cascade reconfigured with a
new motor model returns to the configured bus until the next value, at most 10 ms later.

---

## Interfaces

### Provided

| Interface | Purpose                     | Contract                                                          |
|-----------|-----------------------------|-------------------------------------------------------------------|
| Start     | Begin measuring every 10 ms | The first reading arrives 10 ms after the start and is unfiltered |
| Stop      | Stop measuring              | No reading is handed on after it                                  |

### Required

| Interface            | Purpose                         | Contract                                         |
|----------------------|---------------------------------|--------------------------------------------------|
| Platform bus voltage | Read the bus                    | Called from the event loop; may block for 134 µs |
| Control mode         | Receive each filtered bus value | Called from the event loop                       |
| Timer                | Pace the readings               | —                                                |

---

## Constraints & Limitations

| Constraint     | Description                                                                                                        |
|----------------|--------------------------------------------------------------------------------------------------------------------|
| Tracking lag   | The 10 ms poll and the 50 ms filter follow a slow sag; a bus that moves within a few milliseconds is followed late |
| Blocking read  | The read blocks the event loop for its conversion; a non-blocking read needs a triggered conversion per board      |
| Identification | Resistance, inductance and flux-linkage identification still convert duty to volts with the boot value             |
