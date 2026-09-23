---
title: "Software-in-the-Loop Design"
type: design
status: accepted
version: 1.4.0
component: "software-in-the-loop"
date: 2026-09-23
---

| Field     | Value                       |
|-----------|-----------------------------|
| Title     | Software-in-the-Loop Design |
| Type      | design                      |
| Status    | accepted                    |
| Version   | 1.4.0                       |
| Component | software-in-the-loop        |
| Date      | 2026-09-23                  |

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
| Fault injection   | Open phase per phase, stuck encoder, when it freezes, supply voltage scaling  |
| Reproducibility   | The seed both noise generators start from                                     |
| Disturbance       | A signed shaft torque and the delay after enabling at which it steps in       |
| Recording         | The rate at which the plant reports its trajectory, and how many samples      |

The thermal group's resistance and inductance coefficients act from the temperature the
nameplate values were measured at (25 °C), not from the ambient, so a plant described with a hot
ambient starts with a hot winding.

When no description is present the firmware falls back to the motor it is built with, so the
target still boots standalone. A trip threshold of zero disables that protection, which is how a
scenario opts out of one it is not testing.

The winding and rotor group is filled from one of the two reference motors in `motor_parameters/`
(the Teknic M-2310P-LN-04K on its 40 V bus is the nominal plant, the Anaheim BLY172S-24V-4000 on
24 V the other), whose parameters and sources are in the plant theory document. A scenario names
the motor as a preset; every other preset is a variation on the nominal one. The stored calibration
a scenario boots from is derived from the same description, so "already calibrated" means
calibrated to the plant, and a scenario that wants a wrong seed says by how much.

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

Delivering a board protection fault to a running drive used to lock the firmware up, and the
scenarios that reproduced it were held out of the default run. The fault path defers the state
transition through the weak-pointer event dispatcher, and the emulated platform registered only the
plain one, so the call ran on a singleton that did not exist. On this machine address zero is
writable memory holding the vector table, and the dispatcher's push index sits at offset twelve —
the HardFault vector. The atomic increment advanced that vector from `0x22dd` to `0x22de`, clearing
its Thumb bit; the next store, through a "storage" pointer that was really a code address, took an
unaligned-access fault; and the escalation to HardFault jumped through a vector that could not be
executed. QEMU reported exactly that: a lockup, unable to escalate to HardFault, at `0x22de`.
Registering a dispatcher that serves both singletons (Part J) fixed it; the scenarios were then
found to fail for a second, unrelated reason — two of their thresholds no longer sat on the far
side of the nominal plant after the reference motors replaced the 48 V stand-in — and run in the
default set with those rows corrected.

### Part F — Wiring faults

The plant can be told that a phase carries no current, that the encoder is stuck, or that the
supply sags. These are configuration, not a separate model, and they compose with everything else.

Two results are worth recording because they are not obvious:

- A **fully disconnected** motor produces no torque and does not turn, however it is commanded.
  Its torque follows the currents that actually flow, so masking the phases is enough.
- A motor with **one open phase** still turns. Two phases can produce torque, so this is a
  degraded running condition rather than a dead motor, and nothing in the firmware detects it.
- A **stuck encoder** passes alignment rather than failing it. Alignment declares the rotor
  settled once its reading stops changing for a run of samples, and a reading frozen at one
  value satisfies that on the first run it sees, more convincingly than a real rotor ever
  does. The drive therefore takes its reference from a reading that means nothing and enables
  on it. What exposes it is the first request for motion: the loop pushes torque current and
  the reading does not move. The encoder plausibility monitor (REQ-SM-028) watches for exactly
  that from the event loop — measured torque current above a fraction of the limit, a demanded
  speed or an unclosed position error, and a reading still inside a small excursion for a
  whole window — and raises the sensor fault the wire already carried but nothing used to
  raise. A disconnected motor carries no current and is not misreported; a position held
  against a load demands no motion and is not reported either; a mechanically locked rotor is
  indistinguishable and is reported the same way.

An encoder can also be told to freeze a set time after the motor is enabled rather than to have
been dead all along, scheduled the same way as the shaft torque of Part H and on the same
guest-time base. The two describe different failures and are worth keeping apart: an encoder
already stuck when the drive aligns corrupts the reference the drive takes, while one that
freezes afterwards leaves a valid reference and stops reporting against it. The delay is carried
in hundredths of a second, in a byte the plant description already held in reserve, so a
description written before the freeze existed still reads as one that never freezes and the
record neither grew nor changed version. What freezes is what the firmware reads; the plant goes
on reporting its own trajectory truthfully, which is what lets a scenario see the rotor moving
while the drive does not.

Alignment also cannot converge once encoder noise reaches the threshold below which it declares
the rotor settled, which bounds how noisy an encoder the current calibration tolerates. A frozen
reading and a noisy one therefore fail in opposite directions: the noisy encoder never settles
and refuses to align, the frozen one settles at once and aligns against nothing.

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

Between them those two rules set a ceiling on how long a recording can run, and it is worth
stating because it is not the budget and it bites only the fast ones. The interrupt produces at
the recording rate; the event loop empties a fixed number of records on each of its ticks. A
recording asking for less than that drains as fast as it fills and can run until its budget is
spent. A recording asking for more is safe only while the ring absorbs the difference, and then
begins dropping: at the control rate itself that is roughly a quarter of a second, a few thousand
samples. A scenario measuring a transient from rest never notices, because it records for a few
tens of milliseconds. One measuring a transient that begins half a second after enabling cannot
be run at the control rate at all, whatever budget it is given, and has to record at a rate the
event loop can keep up with instead — which lengthens the span its fixed-size window covers, and
so the capture that waits for that window.

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
normalised before measuring, so a reversal or a step down is the same unit step as a step up.
Settling time, overshoot, steady-state error, rise time and the band the tail still ripples in
are each bounded by the row the scenario carries. Because the step is normalised first, the tail
band is a percentage of the step rather than of the setpoint the step ends at; the two coincide
from rest and differ on a change made while running, where the step is the distance moved and
not the setpoint reached. Peak time is printed and not asserted: a law
that does not overshoot has its largest sample wherever the tail ripple happened to peak, so a
bound on it would measure the duty quantisation rather than the law. A
disturbance is measured as the largest excursion from the setpoint and the time of the last
excursion outside a band around it, which is the same settling computation applied to the
recovery. Every measurement is also printed as a labelled line, which is how limits are found:
a scenario is run first with permissive limits, the printed numbers are read, and the limits are
pinned with a margin. Design intent says where to expect them, roughly two over the loop
bandwidth for settling with a few percent of overshoot, but the pinned numbers come from the run.

One run is not enough to pin from. The same scenario does not measure quite the same numbers
twice, and two machines running it agree less closely still, so a bound is taken from the worst
value several repeated runs report and set at twice that, and the rows are then checked against a
run on the integration machine before they are trusted. A bound sitting on a measured value fails
the next time the measurement moves, and a bound sitting far above every measured value fails
nothing at all; twice the worst of a repeated run is the compromise the suite uses. What the
spread actually is, and why it exists at all against a deterministic guest, is in Part I.

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

The first characterisation run measured the product against the parameter set that stood in
for a motor at the time and reported four findings. Re-measured on the reference motors, after
the harness faults recorded in Part J were fixed, two of them were artefacts and two were real
defects. Later runs found more. Each real defect was root-caused and fixed rather than parked:
every scenario in the suite now runs in the default set.

- **Whole-percent duty resolution — fixed.** The duty cycles the modulator handed the inverter
  were `hal::Percent`, a whole-percent `uint8_t` from the infrastructure library's PWM
  interfaces, and the conversion rounded to the nearest percent. On a 40 V bus into a 0.36 Ω,
  0.20 mH winding one step is 0.4 V and about a tenth of an ampere of ripple per control period,
  so the current loop held a half-ampere setpoint inside a band of about 30 % of it and could
  not settle into a 10 % one. The hardware was no better: the TI driver computed its compare
  value as `load × duty / 100` in integers. The PWM interfaces now take `hal::DutyCycle`, an
  integer Q16 fraction of the period (65536 is 100 %), through the infrastructure library and
  both hardware abstraction layers — integer so that no driver needs floating point, and Q16 so
  that its conversion to counts is a multiply and a shift, never a division, in the interrupt.
  Each driver rounds its compare value with the same `DutyCycle::ToCounts` helper;
  the TI platform also clocks its PWM at half the system clock rather than an eighth, so a
  20 kHz centre-aligned period spans 1500 counts on the 120 MHz part (0.07 % a count) instead
  of 375. The deadbeat law now settles into a 10 % band within a sample with under 5 %
  overshoot, the decoupled law within 1.7 ms with none, and the speed laws' tail band fell to
  0.4 % of a 20 rad/s setpoint.
- **The sliding-mode current law overshot by design — fixed.** Its equivalent control drove the
  next error to zero on its own, and the switching term then threw the error `K_sw` past zero
  every time it left the boundary layer: 40 % of a step with the shipped gains, measured at
  46–50 %. It now uses the exponential reaching law at the loop bandwidth
  (`documentation/theory/current-loop-sliding-mode.md`) and settles into a 10 % band within
  0.2 ms without overshoot.
- **The PID current law keeps a standing error on an accelerating rotor — by design.** A
  torque step accelerates the free rotor at some 2700 rad/s², so the back-EMF ramps at about
  70 V/s, and a PI tracks a ramp with an error of the ramp rate over its integral gain: 0.08 A
  of a 0.5 A step here. Whole-percent dither used to hide some of it. The law has no back-EMF
  feedforward by construction; the decoupled law does, and meets the tight envelope. The PID
  row carries the envelope the law can meet.
- **Two current laws command nothing — retracted.** With the inductance seeded correctly the
  deadbeat law rises within a sample and overshoots 9 %, the sliding-mode law overshoots 50 %
  into its boundary layer, and the decoupled law 25 %. The zero output was the deadbeat and
  sliding designs computed for a winding of half a microhenry.
- **The LQI position law is unstable — retracted.** Same cause. It settles into a 10 % band
  within 30 ms with 27–40 % overshoot, and holds 1.5 rad against a torque step within 0.011 rad,
  the tightest of the position laws.
- **The LQI speed law was erratic — fixed.** From rest to 20 rad/s it overshot 121 % and never
  settled; stepped to 40 rad/s while running it overshot 66 % and kept swinging for the rest of
  the window; a 0.002 N·m torque step pushed it 26 rad/s off a 20 rad/s setpoint, where the other
  laws move less than 2 rad/s. The cause was `LqiSpeedController` never scaling its LQR effort
  weight to the plant's own input gain: the fixed weight sized a design with no margin against a
  real current loop's one-sample delay (`documentation/theory/speed-loop-lqi.md`). Rescaled to the
  plant's input gain and the requested bandwidth, it settles into a 10 % band within 16–19 ms with
  2–4 % overshoot in both step scenarios, and the same torque step moves it about 1.2 rad/s off
  setpoint. Its rows carry the same envelope as the other speed laws now.

A later run, the first to apply a disturbance of either sign, found one more thing and is worth
recording because the asymmetry was not expected:

- **Two position laws rejected a torque better in one direction than the other — fixed.**
  Against a negative torque the PID position loop took 106 ms to return inside the band its
  positive row meets in 50, and the two-DOF loop was pushed 0.1008 rad off a limit of 0.1. The
  "standing error" of some 0.055 rad both showed was not one: their PI placed its integral zero
  at a twentieth of the position bandwidth, which leaves a slow closed-loop pole beside the zero
  and a tail of some 0.089 rad decaying over a second. Half a second after a 1.5 rad step it was
  still 0.055 rad over the setpoint, and a torque pushing the same way crossed the band. The zero
  now sits at a fifth of the bandwidth, and the proportional term weights the reference at three
  quarters so that the zero does not overshoot the step
  (`documentation/theory/position-loop-pid.md`). Both laws now settle a 1.5 rad step within
  0.5 % and are pushed under 0.03 rad by the torque step of either sign.

The same run, once a disturbance was made large enough to reach the current limit, found a
second thing:

- **Two speed laws did not come back from a saturating disturbance — fixed.** A shaft torque of
  half a newton-metre asks for about thirteen of the twenty amperes the drive allows, so the
  current to reject it is there. ADRC and LQI were thrown 205 to 238 rad/s off a 20 rad/s
  setpoint and returned; PI and two-DOF were thrown to about 317 and ended the window
  10.44 rad/s short. There was no windup — the velocity-form PI never reached its clamp. The PI
  placed its integral zero at a tenth of the speed bandwidth, which leaves a slow closed-loop
  pole near 10 rad/s, and the tail mean of that pole's decay over the last quarter of the window
  predicts 10.7 rad/s. The zero now sits at a quarter of the bandwidth
  (`documentation/theory/speed-loop-pi.md`): both laws are thrown about 295 rad/s off and are
  back inside 3 rad/s 175 ms later, and a step from rest overshoots by about 10 %. The scenario
  is the only one in the suite that reaches the current limit at all: every other disturbance
  is kept small on purpose so that it measures the control law rather than the limiter.

How far the limit can be lowered to provoke that is itself bounded, which is worth recording
because it is not obvious: alignment injects open-loop and abandons the attempt the moment a
phase carries more than the drive says it supports, so the limit cannot be dropped below what
that injection draws. On the nominal plant it is above ten amperes, more than half the nominal
twenty. A limit low enough to make a millinewton-metre step saturate stops the motor aligning
at all, so a scenario that wants saturation raises the torque rather than lowering the limit.

The run also found a harness fault: a line cut by a read timeout was dropped and its tail
parsed as a line of its own, which showed up as a gap in the sample spacing. The reader now
keeps a partial line for the next read.

Two runs of the same scenario do not measure exactly the same numbers, and how far they differ
has now been measured rather than assumed. Six consecutive runs of the default set passed every
scenario in every run — six hundred and seventy-two executions with no failure of any kind, so
the suite itself does not flake — and yet only a fifth of the measurements taken from rest
repeated exactly across all six, and none of the measurements taken from a setpoint changed while
running did. The guest is deterministic; the host decides when each command frame reaches it, so
the tick a setpoint or an enable lands on, relative to the outer loop's phase, moves between runs
and the transient moves with it. The onset of one mid-run change moved by a sixth of a second of
guest time across the six.

What moves is small on most rows and not on all of them. Rise times moved by at most five
milliseconds and tail bands by about a point, while one measurement moved far further: the
current loop's reversal ripples in a band eleven points wide from run to run, between 20 and 31
percent of its step. Two machines agree less closely again. The same reversal's overshoot has
measured between 9 and 13 percent on a development machine and between 10 and 15 percent on the
integration machine, so the range either one sees alone understates the range the scenario has.

That is what sets the margin on a pinned limit, and it is why the rise and tail bounds are twice
the worst of six runs rather than a tighter multiple of one. It leaves the widest spread yet
measured several times over, and room for a machine nobody has measured on. Tightening those
bounds towards the measurement would buy a sensitivity the suite cannot support, and the first
thing it would catch is the harness rather than a control law.

The two runs made to check the pinned bounds bear that out immediately: three of their
measurements came in above the worst of the six the bounds were set from, the largest a tail band
a fifth higher than anything those six had shown. Every bound still had more than 46 percent of
itself to spare, and a bound pinned to the measurement itself would have been red on all three,
on the very run that was meant to confirm it.

### Part J — The estimators against a known plant

A simulated plant is the one place where an estimator can be marked against the truth: the
scenario wrote the resistance, inductance, flux linkage, inertia and friction the plant runs
with, so whatever the firmware identifies can be compared with them, and the comparison is
made on both reference motors so that a tolerance is not tuned to one winding.

Three product channels carry what the firmware believes, and all three are the product's own:

- The **electrical parameters response** the CAN identification command broadcasts before it
  acknowledges: resistance, inductance and pole pairs at the wire's resolution of 1 mΩ and 1 µH.
- The **calibration record** the state machine traces whenever it stores one, `[SM] Calibration
  record:`. This is the only way the mechanical identification's result is visible on this target:
  the CAN mechanical command is not wired, and the full sequence that includes it runs from the
  terminal's `calibrate`, which in the emulator travels on the socket the CAN frames use.
- The **online estimates** the terminal prints on `estimate_status`, `[EST] Mech:` and
  `[EST] Elec:`, seeded from the record and updated at the outer-loop rate while the motor runs.

Inertia and friction are traced in micro-units on both trace lines; the tracer prints three
decimals, and the rotors here weigh a few micro-newton-metre-seconds-squared.

The scenarios cover the offline procedures from a blank calibration and from one deliberately
wrong by given factors, the online estimators from a wrong seed under a speed reference that
alternates between two levels (the same shape the identification service uses, because a constant
speed excites neither inertia nor friction), the same with a constant shaft torque applied while
the estimators run, and a winding that heats while running. The excitation is paced on the plant's
own clock, read from its trajectory samples, so it lasts the same guest time on every host.

What the first characterisation found, in the order it was found:

- **The emulated inverter delivered current samples at the control rate whatever rate a
  procedure asked for.** The identification procedures ask for 10 kHz and demodulate on that
  assumption, so every sample arriving twice as often made the injection twice the frequency it
  was analysed at, and the inductance came out doubled on both motors (0.36 mH for a 0.20 mH
  winding, 1.22 mH for 0.64 mH). The platform now delivers samples at the requested rate, as an
  ADC trigger divider would, while the plant keeps stepping at the control rate. With that, the
  resistance identifies within 2 % and the inductance within 15.5 % (Teknic) and 6.5 % (Anaheim),
  reading low in both cases, which is the ZOH bias the electrical theory document predicts made
  larger by the whole-percent duty quantising the injection. Pole pairs identify exactly. None of
  these numbers move under current and encoder noise.
- **The mechanical identification could never complete on the emulated target.** Its completion
  is deferred to the event loop against a weak pointer, so that an identification torn down
  before the completion runs is dropped rather than executed on a dead object. The emulated
  platform registered only the plain dispatcher, and the weak-pointer dispatcher is a second
  singleton next to it; scheduling through it dereferenced nothing and hard-faulted inside the
  control interrupt on the first sample the identification observed. The fault handler then
  faulted again, so nothing was traced and the target simply went silent. The deferred fault
  notifications take the same path. The platform now registers a cortex dispatcher that serves
  both singletons. Of the hardware platforms, the TI one registers the weak-pointer dispatcher;
  the ST one is a stub that registers no dispatcher at all and would fail the same way once it
  drives a motor. With the
  dispatcher in place the full calibration from the terminal reaches Ready on both motors and
  its record is within 0.05 % of the plant's inertia and 2 % of its viscous friction, from a
  seed that was off by a factor of two and a half; the offline procedure, sampling at the control
  rate, is as good as the plant it measures.
- **The stored calibration the scenarios booted from carried the inductance in henry where the
  record holds millihenry**, so every "already calibrated" run since the plant description was
  introduced configured its current loop with an inductance a thousand times too small. Part I's
  numbers were measured that way and are re-measured below.
- **The torque constant was a per-target constant of 0.1 N·m/A**, two and a half to three times
  the true value of either reference motor, used by the mechanical identification, the online
  inertia estimator and the speed-loop gain design alike. It is now derived from the calibration
  record (REQ-SM-027).
- **Small mechanical values did not survive the product's own outputs.** The mechanical
  parameters response on the wire carries inertia and friction as fixed-point with a resolution
  of 1e-4, so the rotors here (5e-6 to 7e-6 kg·m², 1e-5 N·m·s/rad) encode as zero; that
  response is not used by the scenarios and changing its scale is a wire-contract change left
  open. The tracer prints three decimals, so the same values traced as zero; the traces now
  carry them in micro-units.
- **The online mechanical estimator did not track.** Seeded with twice the inertia and half the
  friction and excited for six seconds by a speed reference alternating between 26 and 52 rad/s
  every quarter second, it published an inertia 26–33 % low and a friction near zero on the
  Teknic, and nothing at all on the Anaheim. It regressed an instantaneous current sample against
  the second difference of two window-mean speeds, which is centred an outer sample earlier; with
  the speed loop's transients decaying at some 400 rad/s that alone scaled the inertia by about
  two thirds. Its 200 ms memory was shorter than one 250 ms dwell, so within its window friction
  was collinear with the intercept. Its excitation gate was met by duty ripple on every plateau,
  and its convergence gate — an innovation under 0.1 mN·m — sat under that same ripple and was
  met by chance. It now regresses the momentum balance over window averages the control interrupt
  accumulates (theory: `friction-inertia-estimation.md` §4.1) with a two-second memory, and
  publishes once its averaged residual is under 1 % of the torque it explains.

  Fixing that exposed a platform-independent timing fault: the outer loop measured speed by
  differencing whatever angle it found when it ran, and it runs up to a tick after the window
  closes, so windows of 19 and 21 ticks were divided by 20. The jitter — some 2000 rad/s² of
  acceleration noise — biased the inertia low by errors in the regressor, and it also reached the
  speed loop. The interrupt now latches the angle at the window boundary. Both reference motors
  then identify inertia within 4 % and friction within 8 %, and a constant 0.01 N·m shaft torque
  lands in the intercept (10.0 mN·m) instead of the friction.

- **The online electrical estimator could not see the resistance.** Its regressor's resistance
  column was the d-axis current, which the product regulates to zero, so that direction was
  never excited and the recursion drifted: −0.68 Ω for a 0.36 Ω winding, −1.63 Ω for 0.405 Ω. It
  also paired each current with the voltage commanded on the same tick, which contains the PI's
  own −Kp·id and pulled the resistance negative by about Kp, and it differentiated two
  instantaneous currents a millisecond apart. The cascades now carry a d-axis square wave while
  the estimator is attached (±2.5 % of the current limit at 10 Hz, no torque on a non-salient
  machine), and the estimator regresses the integrated d-axis equation over window averages that
  pair each current with the voltage applied while it formed (`service-electrical-ident.md`).
  The resistance then reads 0.360 Ω and 0.405 Ω.

- **The plant counted the frame rotation twice.** With the resistance fixed the Anaheim
  inductance still read 48 % high, and the window residuals were exactly −L·ωe·iq through every
  speed transition. The plant integrates the dq equations — which already carry the ±ωL coupling
  the rotating frame produces — then converted the new currents back to phase currents at the
  angle the step *started* from, and the next step read them at the new angle: the rotation was
  applied twice and the plant's cross-coupling was doubled. Every scenario since the plant model
  was introduced ran against that doubled coupling. The currents now return to phase quantities
  at the angle the step ends on, and the inverter's stator-fixed voltage is read at mid-step. The
  inductance then reads 0.191 mH and 0.636 mH.

- **The heated winding could not be tracked as described.** The plant scaled its resistance from
  its ambient temperature, and the "hot" preset set the ambient to 90 °C, so its winding
  resistance stayed at the 25 °C nameplate value while the scenario compared against the value at
  90 °C. The thermal model now carries the temperature the nameplate values were measured at, and
  the heated winding reads 0.453 Ω against 0.452 Ω.

- **The offline friction depended on the duty ripple.** With the duty carried as a fraction the
  full calibration's friction moved 19–27 % off the plant. Its excitation gate refused every
  sample accelerating slower than 1 rad/s², which on a clean plateau is every sample, and the
  plateaus are what separate friction from the intercept; only the whole-percent ripple had let
  them through. Both procedures now take every sample above standstill and require a span of
  speeds before publishing (`service-mechanical-ident.md` § *Persistence of Excitation*).

Every identification scenario, offline and online, runs in the default set on both motors
(REQ-CAL-012 to REQ-CAL-014).

---

---

## Scenario Taxonomy

| Area                     | What it establishes                                                                                               |
|--------------------------|-------------------------------------------------------------------------------------------------------------------|
| Control modes            | Torque, speed and position each align, enable, take a setpoint, disable                                           |
| Controller algorithms    | Every algorithm of every loop runs, plus combinations across the loops                                            |
| Plant characteristics    | Control holds up across noise, temperature, load and a different winding                                          |
| Wiring faults            | A dead motor does not turn; a degraded one does; a stuck or frozen encoder is reported as a sensor fault          |
| Memory integrity         | Damaged calibration is distrusted; damaged configuration falls to defaults                                        |
| Board protection         | Trips reach the state machine and are reported                                                                    |
| Control performance      | Each loop's step response stays inside its settling, overshoot and error envelope, from rest and while running    |
| Control robustness       | The same response measured on a noisy, hot, loaded or differently wound plant, and the law reported still running |
| Disturbance rejection    | A shaft torque step while regulating is bounded in excursion and recovered from                                   |
| Parameter identification | What the firmware identifies offline, and tracks online, matches the plant it was given, on both reference motors |

Controller coverage sweeps each loop's algorithms with the other loops held at the baseline, and
adds a handful of combinations chosen to exercise both kinds of position law: those that produce a
speed reference and run through the speed loop, and those that produce a current reference and
bypass it. The full cross product is an order of magnitude more scenarios for coverage the sweep
already provides.

---

## Interfaces

**Provided to scenarios:** a named plant (one of the reference motors, or a variation on the
nominal one) or one described field by field, a stored calibration and configuration to boot
from, a calibration deliberately off by given factors, a scheduled shaft torque, a recording rate
and budget, a boot step, the CAN command set, the product's terminal commands, a capture step that
waits on the plant's own clock, and assertions over telemetry, reported algorithms, fault codes,
rotor movement, step-response metrics, disturbance recovery and identified or tracked parameters.

**Required from the target:** that it read its plant description at boot, expose its state, fault
code, measured speed and position over telemetry, trace the algorithm each loop is running, trace
every calibration record it stores and its online estimates on request, and, when asked to, report
the plant's trajectory and the tick each command frame was delivered on.

**How a terminal command reaches the emulated target:** on the same socket as the CAN frames. The
firmware reads that socket line by line; a line with the `CAN_RX` prefix is a frame, any other line
is handed to the terminal exactly as a serial port would hand it. The terminal answers through the
tracer, so a step that needs the outcome drains the trace for the line it expects instead of waiting
for a prompt.

**Not required:** any test-only command. The target's CAN and terminal surfaces are the product's.
