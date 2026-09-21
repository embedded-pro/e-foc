---
title: "Software-in-the-Loop Design"
type: design
status: accepted
version: 1.1.0
component: "software-in-the-loop"
date: 2026-09-21
---

| Field     | Value                       |
|-----------|-----------------------------|
| Title     | Software-in-the-Loop Design |
| Type      | design                      |
| Status    | accepted                    |
| Version   | 1.1.0                       |
| Component | software-in-the-loop        |
| Date      | 2026-09-21                  |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
>
> **Diagrams**: All visuals must be Mermaid or ASCII art. External image references are **not allowed**.

---

## Overview

Software-in-the-loop runs the **real firmware** for the emulated Cortex-M4 target under an
emulator, against a simulated three-phase motor that the firmware itself owns. The test host
speaks to it only over the wire the product speaks: CAN frames carried as text lines on the
emulated UART, plus the firmware's own trace output.

Nothing is mocked on the target. The state machine, the control cascades, the calibration
services, the non-volatile memory stack and the CAN server are the shipping ones. Only the motor,
the power stage and the board protection are simulated, because there is no hardware.

```mermaid
graph LR
    subgraph HOST["Test host"]
        GHERKIN["Gherkin scenarios"] --> STEPS["Step definitions"]
        STEPS --> SESSION["Emulator session"]
    end
    subgraph TARGET["Emulated target — real firmware"]
        PLANT["Motor plant model"] --> PLATFORM["Platform"]
        PROT["Board protection"] --> PLATFORM
        PLATFORM --> APP["State machine, cascades, NVM, CAN"]
    end
    SESSION -->|"plant description"| PLANT
    SESSION -->|"NVM image"| APP
    SESSION <-->|"CAN frames, traces"| APP
```

---

## Responsibilities

**Is responsible for:**
- Describing the motor plant per scenario and delivering that description to the firmware at boot
- Pre-seeding non-volatile memory so a scenario starts from a chosen calibration and configuration
- Driving the target only through its CAN command set and observing only its telemetry and traces
- Simulating the board protection the hardware provides, and the wiring faults hardware can suffer
- Isolating scenarios from one another so neither files nor sockets nor captured output leak across

**Is NOT responsible for:**
- Verifying control algorithm correctness in the small — that is what the unit tests are for
- Testing CAN framing — that is covered by the wire contract tests
- Standing in for hardware-in-the-loop, which alone exercises real silicon, timing and analogue paths

---

## Component Details

### Part A — The plant is an input, not a constant

The motor the firmware drives is described per scenario and handed to it at boot as a small
binary record placed in the emulator's working directory. The record carries a magic number, a
layout version and a checksum over its payload, exactly as the non-volatile memory records do, so
a truncated or stale description is rejected rather than half-read.

The description covers everything the plant needs:

| Group             | Contents                                                                      |
|-------------------|-------------------------------------------------------------------------------|
| Winding and rotor | Resistance, both axis inductances, flux linkage, pole pairs, inertia, damping |
| Drive             | Supply voltage, control frequency, peak current, load torque                  |
| Measurement       | Current noise deviation and per-phase bias, encoder noise deviation and bias  |
| Thermal           | Ambient, thermal resistance and capacitance, copper and iron coefficients     |
| Protection        | Over-current, over-voltage, under-voltage and over-temperature trips          |
| Fault injection   | Open phase per phase, stuck encoder, supply voltage scaling                   |
| Reproducibility   | The seed both noise generators start from                                     |
| Disturbance       | A signed shaft torque and the delay after enabling at which it steps in       |
| Recording         | The rate at which the plant reports its trajectory, and how many samples      |

When no description is present the firmware falls back to the motor it is built with, so the
target still boots standalone. A trip threshold of zero disables that protection, which is how a
scenario opts out of one it is not testing.

The seed matters more than it looks: the emulated target cannot obtain entropy, so without an
explicit seed the noise generators are not merely unpredictable, they are unreliable.

### Part B — Reaching the target

The emulator runs from a directory created for the scenario, so the firmware opens its plant
description and its memory image by plain relative name and knows nothing about the host's paths.
The directory, and the socket carrying CAN frames, are unique per session; scenarios ran into each
other when either was shared.

```mermaid
sequenceDiagram
    participant S as Scenario
    participant H as Host session
    participant T as Target
    S->>H: describe the plant
    S->>H: describe the stored calibration and configuration
    S->>H: boot
    H->>H: write both images into the scenario directory
    H->>T: start the emulator there
    T->>T: read the plant, restore the configuration
    T-->>H: ready
    S->>T: CAN commands
    T-->>S: telemetry and traces
```

### Part B2 — Emulated time, and why it has to be instruction-counted

By default the emulator's guest clock follows host wall-clock while the guest's instruction
throughput follows whatever CPU the host happens to give it. The firmware's timers therefore keep
real time regardless of how much work the emulated core actually completed.

That breaks the control system's own supervision. The control interrupt runs at 20 kHz, and on
this machine it carries the whole motor plant as well as the control loop, which no emulated core
of this class completes in a 50 microsecond period. The interrupt then crowds out the 1 kHz outer
loop, which runs at the lowest priority. The supervisor samples both loops and refuses to feed the
watchdog when the outer one has not progressed; four consecutive refusals expire the deadline and
emergency-stop a motor that was running perfectly well.

This is the supervision working correctly on a target that genuinely cannot keep up. It was
measured, not inferred: every refusal reported the inner loop progressing and the outer loop not,
in speed and position modes only — the two modes whose supervision requires the outer loop.

The emulator is therefore run with its virtual clock tied to instructions retired rather than to
host time, at 8 nanoseconds per instruction. That models a core of roughly 125 MHz, at least as
fast as the slowest target the firmware ships on, so work that fits in the emulator fits on
hardware. It also makes a run's timing independent of host load, which is what removes the
flakiness rather than merely reducing it.

> The emulated machine's own clock is 25 MHz and the platform must declare exactly that. The
> declaration is a divisor used to program the timers, not a statement of capability: declaring a
> faster core does not execute more instructions, it just makes every firmware timer run
> proportionally slower than the firmware believes. Declaring 50 MHz, for instance, was measured
> to halve the rate of the firmware's own clock. Capacity comes from the instruction-counted
> clock above; the declared value must stay matched to the machine.

### Part C — Starting from a chosen calibration

Reaching a mode that regulates speed or position requires a complete calibration, including the
mechanical parameters. Rather than run identification in every scenario, the harness writes a
memory image whose calibration record already satisfies every completeness check. The target then
boots straight to Ready, and one alignment command establishes the rotor reference the state
machine requires before enabling.

This cuts a scenario from the better part of a minute to a few seconds, and it is also what makes
the damaged-record scenarios possible: the same builder can produce a record with a wrong magic
number, a stale layout version or a broken checksum, and the scenario asserts what the firmware
does with it.

### Part D — Selecting controller algorithms

Algorithm selection is deliberately not exposed over CAN. The harness therefore does not ask for
an algorithm; it writes the stored configuration that the firmware restores on entry to Ready, and
then asks the firmware which algorithm is actually running.

That last step is not ceremony. A state feedback design that does not converge for a given motor
leaves the previously active algorithm in place. A scenario that only asserted what it requested
would pass while testing something else entirely, so the firmware reports the algorithm each loop
ended up with and the scenario compares against that.

```mermaid
stateDiagram-v2
    [*] --> Stored: harness writes the configuration
    Stored --> Restored: target enters Ready
    Restored --> Active: design accepted
    Restored --> Unchanged: design rejected
    Active --> Reported
    Unchanged --> Reported: previous algorithm still running
    Reported --> [*]: scenario compares against what runs
```

### Part E — Board protection

Real boards compare bus voltage and phase current against thresholds in hardware and feed the
result to the power stage's fault input. The simulation mirrors that arrangement in three ways
that matter:

- It is **armed with the inverter**, because comparators guard a stage that is switching. A
  standing condition does not fault a motor that was never enabled.
- It is **evaluated every control tick**, against the live plant state, so a trip follows from the
  physics of the scenario rather than being poked in by the test.
- It is **delivered from the event loop**, not from the control interrupt. The handler stops the
  drive and walks the state machine, which is far more than belongs in a 20 kHz interrupt; on
  hardware the equivalent work is done from a dedicated fault interrupt.

A condition already standing when the state machine registers its handler is still reported. An
edge that passes before anyone is listening would otherwise be lost, and the motor would enable
into a fault the board had already detected.

> **Known defect.** Delivering a board protection fault to a running drive currently locks the
> firmware up: the fault path takes an unaligned-access exception, which faults again and
> escalates. The path had never been exercised, because the emulated platform ignored protection
> registration entirely and reported its state as unknown, and no other platform raises it under
> test. The scenarios that reproduce it are kept, tagged apart from the default run.

### Part F — Wiring faults

The plant can be told that a phase carries no current, that the encoder is stuck, or that the
supply sags. These are configuration, not a separate model, and they compose with everything else.

Two results are worth recording because they are not obvious:

- A **fully disconnected** motor produces no torque and does not turn, however it is commanded.
  Its torque follows the currents that actually flow, so masking the phases is enough.
- A motor with **one open phase** still turns. Two phases can produce torque, so this is a
  degraded running condition rather than a dead motor, and nothing in the firmware detects it.

Alignment also cannot converge once encoder noise reaches the threshold below which it declares
the rotor settled, which bounds how noisy an encoder the current calibration tolerates.

### Part G — The plant reports its trajectory

Telemetry cannot time a transient. The status frame is answered on request, so the host samples
at whatever cadence its round trips allow, tens of milliseconds apart; the speed it carries is
quantised to whole radians per second; and the host's own clock says nothing about the guest's,
because the emulated clock counts instructions rather than seconds. A speed loop that settles in
a few tens of milliseconds is invisible through that window.

The plant therefore reports what it did, itself. Every control tick, once the motor is enabled,
the plant samples its mechanical speed, its mechanical angle, its d- and q-axis currents and the
external torque acting on the shaft, decimated to the rate the plant description asks for, and
stamps each sample with the control-tick count. The tick count is the only time base that means
anything on this target: it advances exactly once per control period whatever the host is doing.

```mermaid
sequenceDiagram
    participant I as Control interrupt
    participant R as Ring
    participant E as Event loop
    participant H as Host
    I->>R: sample every Nth tick, stamped
    E->>R: drain a few records per millisecond
    E-->>H: one text line per record on the trace channel
    H->>H: parse into a series, check the spacing
```

Two rules keep this honest. Nothing is printed from the interrupt: samples go into a ring the
interrupt alone writes, and the event loop alone drains, so the trace channel is written from one
context as it always was. And the ring never blocks: when it is full the sample is dropped and
counted, the count is reported when recording stops, and the host also checks that consecutive
samples are exactly one decimation apart. A gap fails the scenario rather than skewing a metric.

Recording begins on enable, which resets the rotor to rest, and stops when the sample budget in
the plant description is spent or the motor is disabled. The budget bounds the output so a
scenario that never reads it cannot fill the pipe and stall the guest; a scenario that measures
drains continuously while it waits. Marker lines bracket the run: one when recording begins with
the tick it began on, one when a scheduled torque step fires, one when recording stops. Every
command frame the target takes from the wire is stamped with the tick it was delivered on, so a
setpoint changed while running has an onset the host can look up rather than guess.

The measurements themselves are the classical ones and are computed on the host with the
numerical toolbox's step-response metrics: rise time, settling time into a two-percent band,
percent overshoot, peak time and steady-state error over the tail of the window. A step is
normalised before measuring, so a reversal or a step down is the same unit step as a step up. A
disturbance is measured as the largest excursion from the setpoint and the time of the last
excursion outside a band around it, which is the same settling computation applied to the
recovery. Every measurement is also printed as a labelled line, which is how limits are found:
a scenario is run first with permissive limits, the printed numbers are read, and the limits are
pinned with a margin. Design intent says where to expect them, roughly two over the loop
bandwidth for settling with a few percent of overshoot, but the pinned numbers come from the run.

### Part H — Disturbances are described, not poked

The load torque the plant has always carried opposes motion, so it flips sign with the speed and
vanishes at rest, like friction. That is the right model for a loaded shaft and the wrong one for
a disturbance: a position loop holding still against it sees only a dither around zero. The
plant therefore also carries a signed shaft torque, which keeps its sign whatever the rotor does,
like gravity on an arm.

A scenario schedules that torque in the plant description: a magnitude and a delay after
enabling. The control tick applies it on the exact tick, notes the tick on the trace, and the
loop under test has to hold its setpoint against it. Re-enabling re-arms the schedule and
disabling clears the torque, so every enable starts from the same plant.

Setpoint changes need no such machinery. The product accepts a setpoint while running and while
ready, and every cascade re-applies its last setpoint on enable. A scenario that applies the
setpoint before enabling makes enabling the step, with the onset on the tick recording began and
the rotor known to be at rest. A scenario that applies one while running reads the onset from the
delivery stamp. The target's command surface is still the product's: nothing was added to it.

### Part I — What the measurements found

The first characterisation run measured the product as it is, and three of its findings are
defects rather than tuning. The scenarios that expose them are kept, with the limits the law
should meet, under a tag held out of the default run in the same way as the protection
scenarios; the scenarios in the default run carry the envelope the product holds today, so a
regression is still caught while the defects are open.

- **Whole-percent duty resolution.** The duty cycles the modulator hands the inverter are
  three whole-percent values, and the conversion rounds to the nearest percent. On a 48 V bus
  into a 73 mΩ winding one step is half a volt and several amperes, so the current loop cannot
  hold a half-ampere setpoint: it sits at zero until its integrator has wound up a whole step,
  then saws between adjacent steps. The speed loop above it inherits the ripple as a limit cycle
  of about a quarter of a 20 rad/s setpoint, although its mean stays within a fraction of a
  radian per second. Every loop is affected on every platform, because the type is the product
  interface's. Until the duty carries more resolution, settling into a tight band cannot be
  required of any loop.
- **Two current laws command nothing.** With the deadbeat or the sliding-mode law selected, and
  the firmware confirming the selection, the q-axis current stays at zero for the whole window
  after enabling while the residual d-axis current decays freely. The decoupled law does drive
  the motor but overshoots the step more than twofold.
- **The LQI position law is unstable on the nominal plant.** Enabled against a 1.5 rad
  setpoint it runs away within a hundred milliseconds, with the speed swinging hundreds of
  radians per second in both directions and the current saturating either way.
- **The LQI speed law is erratic on a setpoint change while running.** From rest to 20 rad/s it
  behaves like the other laws, limit cycle included. Stepped from 20 rad/s up to 40 rad/s it
  overshoots the step almost threefold and keeps swinging by tens of radians per second around
  a mean that is nonetheless right; reversed from 20 rad/s to -20 rad/s it settled in a few
  milliseconds in one run and not at all within half a second in the next.

The run also found a harness fault: a line cut by a read timeout was dropped and its tail
parsed as a line of its own, which showed up as a gap in the sample spacing. The reader now
keeps a partial line for the next read.

Two runs of the same scenario do not measure exactly the same numbers. The guest is
deterministic, but the host decides when each command frame reaches it, so the tick on which a
setpoint or an enable lands relative to the outer loop's phase moves from run to run and the
transient with it: the same step measured a 40 % overshoot in one run and 43 % in the next.
The pinned limits carry a margin for that, and a limit that sits on a measured value is wrong.

---

## Scenario Taxonomy

| Area                  | What it establishes                                                                                            |
|-----------------------|----------------------------------------------------------------------------------------------------------------|
| Control modes         | Torque, speed and position each align, enable, take a setpoint, disable                                        |
| Controller algorithms | Every algorithm of every loop runs, plus combinations across the loops                                         |
| Plant characteristics | Control holds up across noise, temperature, load and a different winding                                       |
| Wiring faults         | A dead motor does not turn; a degraded one does                                                                |
| Memory integrity      | Damaged calibration is distrusted; damaged configuration falls to defaults                                     |
| Board protection      | Trips reach the state machine and are reported                                                                 |
| Control performance   | Each loop's step response stays inside its settling, overshoot and error envelope, from rest and while running |
| Disturbance rejection | A shaft torque step while regulating is bounded in excursion and recovered from                                |

Controller coverage sweeps each loop's algorithms with the other loops held at the baseline, and
adds a handful of combinations chosen to exercise both kinds of position law: those that produce a
speed reference and run through the speed loop, and those that produce a current reference and
bypass it. The full cross product is an order of magnitude more scenarios for coverage the sweep
already provides.

---

## Interfaces

**Provided to scenarios:** a named plant or one described field by field, a stored calibration and
configuration to boot from, a scheduled shaft torque, a recording rate and budget, a boot step,
the CAN command set, a capture step that waits on the plant's own clock, and assertions over
telemetry, reported algorithms, fault codes, rotor movement, step-response metrics and
disturbance recovery.

**Required from the target:** that it read its plant description at boot, expose its state, fault
code, measured speed and position over telemetry, trace the algorithm each loop is running, and,
when asked to, report the plant's trajectory and the tick each command frame was delivered on.

**Not required:** any test-only command. The target's CAN surface is the product's.
