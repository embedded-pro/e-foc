---
title: "Service: Motor Alignment"
type: design
status: draft
version: 0.4.1
component: service-alignment
date: 2026-11-26
---

| Field     | Value                    |
|-----------|--------------------------|
| Title     | Service: Motor Alignment |
| Type      | design                   |
| Status    | draft                    |
| Version   | 0.4.1                    |
| Component | service-alignment        |
| Date      | 2026-11-26               |

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
- Calibrating the encoder zero-offset so that the encoder frame and the motor's electrical rotor frame are aligned (position = 0 in the encoder corresponds to electrical angle = 0° in the rotor)
- Applying an open-loop DC voltage vector at electrical angle 0° via the three-phase inverter to force the rotor to a known magnetic position
- Detecting rotor settlement by monitoring encoder position change over consecutive samples
- Reporting the calibrated encoder offset (in radians) via a single completion on success, or reporting failure if the rotor does not settle within the allowed time
- Refusing to align a motor whose phases do not carry the current the alignment vector drives, and saying so (REQ-PRO-005): a disconnected motor or an open phase leaves the rotor where it is, which the settlement check alone takes for an aligned rotor
- Refusing to align when the encoder does not follow the field (REQ-CMS-001): the vector is held a quarter electrical turn ahead first, and the encoder must travel the matching mechanical angle when the vector moves to the alignment angle. An encoder stuck from boot reads as settled at once, which the settlement check alone takes for an aligned rotor
- Stopping the inverter cleanly after the procedure completes, whether successfully or not
- Enforcing mutual exclusion: silently ignoring re-entry while an alignment is already in progress

---

## Component Details

### Preconditions and Configuration

Before invoking alignment, the caller must have:

1. Stopped the normal FOC loop so the three-phase inverter is not claimed by a current control loop.
2. Provided the number of pole pairs for the motor under test.
3. Given an alignment configuration that holds:
   - **test voltage percent** — fraction of DC bus voltage (0–100 %) applied to the d-axis during the procedure
   - **sampling frequency** — the rate at which encoder samples are taken and settlement is checked (Hz); a
     divisor of the switching frequency, since the shared [decimating inverter](platform-phase-current-rate.md) hands
     on every Nth sample while the power stage keeps switching at its own frequency
   - **max samples** — the total number of sampling ticks before the procedure times out
   - **settled threshold** — the maximum absolute encoder position change (radians) that qualifies as "not moving"
   - **settled count** — the number of consecutive below-threshold samples required to declare settlement
   - **phase resistance** and **bus voltage** — the calibrated winding resistance and the measured supply, from
     which the phase-continuity check derives the current the vector should drive; either left at zero skips the
     part of the check that needs them
   - **phase inductance** — the calibrated winding inductance; with the resistance it sets how far the current
     can have risen by the settled samples. Left at zero, the current is taken as settled
   - **follow step** — the electrical angle ahead of the alignment angle at which the vector is held first, so
     the encoder can be seen to follow the field; zero skips the first hold and the follow check. The calibration
     flow passes a quarter turn
   - **follow dwell** — the samples of the second hold during which settlement is not yet counted, so the rotor
     has started to move before it can be declared settled; 100 by default

These parameters are tuned by the application engineer for the specific motor and mechanical load. A typical configuration applies 10–20 % of bus voltage and requires 20 consecutive samples within 0.001 rad of one another before declaring settlement.

### Open-Loop Voltage Application

The alignment service drives the inverter directly in open-loop mode, bypassing all current feedback. It synthesises a stationary voltage vector by computing the inverse Clarke and inverse Park transforms with:

- d-axis voltage = test voltage percent / 100 (normalised to DC bus)
- q-axis voltage = 0 (no torque component)
- electrical angle = 0°, or the follow step during the first hold

The resulting three-phase duty cycles are submitted to the inverter each sampling tick for the duration of the procedure. Because the q-axis component is zero, the resulting stator field is purely aligned with the d-axis at 0°. A surface PMSM or BLDC rotor will rotate to align its permanent-magnet d-axis with this field and remain there.

```mermaid
flowchart LR
    CFG["Alignment configuration\n+ pole pairs"] --> VOLT["Compute open-loop\nvoltage vector\nVd=test%, Vq=0, θe=0°"]
    VOLT --> INV["Three-phase inverter\napply duty cycles"]
    INV --> MOTOR["Motor rotor\nrotates to θe=0°"]
    MOTOR --> ENC["Encoder\nread position"]
    ENC --> SETTLE["Settlement\ndetector"]
    SETTLE -->|"converged"| CB["Complete with the offset"]
    SETTLE -->|"timeout"| FAIL["Complete with no offset"]
```

### Settlement Detection

Settlement detection is a simple sliding window state machine. At each sampling tick (paced by the inverter's phase-current interrupt):

1. The encoder position is read.
2. The absolute difference between the current and previous position sample is computed.
3. If this difference is below the settled threshold, the consecutive-settlement counter is incremented; otherwise the counter is reset to zero.
4. If the counter reaches the settled count, the rotor is declared settled.
5. If the total sample count reaches the max samples without settlement, a timeout is declared.
6. If any phase current in a sample exceeds the inverter's maximum supported current, the injection is aborted immediately and the timeout path is taken. Alignment drives an open-loop voltage with no current regulation, so this per-sample check is the only software limit on the resulting current.

Sampling is gated on the pending completion, not on the release of the inverter's sample handler.
Releasing that handler from inside its own run would destroy the handler that runs. The over-current path
fails from inside it. A sample arriving after the procedure ended is discarded before it reaches anything.

The abort is the caller-facing form of the same shutdown. It stops the alignment voltage and drops the pending
completion **without invoking it**. Thus an alignment result that arrives after a fault raised by the state
machine cannot overwrite that fault.

The sampling interrupt decides a maximum-sample run-out and an over-current, and stops the inverter there.
It does not decide a settled rotor. The interrupt only hands the settled samples to the event loop. The event
loop judges the phase currents. While the vector still holds the rotor, the event loop then zeroes the encoder
and stops the inverter (Phase Continuity Check). The completion is never delivered from the interrupt either.
The caller stores calibration data and changes state from it. This is event-loop work and far longer than a
control period, so the outcome is queued and delivered from the event loop. Until then the run is still in
progress: samples are discarded, a second alignment request is rejected, and an abort drops the queued outcome.
The outcome carries the run it belongs to, so one that an abort overtook cannot complete a run started after
it. The step timeout already runs on the event loop and completes the run directly. If an outcome is already
queued, the step timeout leaves it to be delivered.

The encoder position at the moment of settlement declaration becomes the calibration offset. This offset represents the mechanical angle the rotor adopts when the electrical d-axis is at 0°. Subtracting it from any subsequent encoder reading, and wrapping the difference to [−π, π), yields the corrected angle for the closed-loop FOC controller.

The offset is in **mechanical** radians, which is the unit the encoder's set and zero operations take. It is not scaled by pole pairs; scaling it would make it an electrical angle and misorient the field.

The service applies the offset itself. It zeroes the encoder once the settled run is judged, while the rotor
is still held at electrical zero. It does not wait for the caller to re-apply the reported value, because the
rotor drifts as soon as the bridge is released. The reported value is therefore a record of where the encoder
read before it was zeroed. The calibration record persisted to the non-volatile memory is diagnostic. An
incremental encoder loses its reference across a reset, so the state machine does not re-apply a stored
alignment at boot. What survives a reset is the angle of the encoder's index in the frame the alignment sets.
The calibration measures this angle right after the alignment with an index search (`service-index-search.md`).
A drive whose record holds one restores the frame from the index at its first Enable instead of aligning again.
Without an index, the alignment must be re-run after every power-up to orient the field.

### Phase Continuity Check

A rotor held at electrical zero by a vector the windings do not carry looks exactly like an aligned rotor.
Its reading stops changing at once. So the service checks the phase currents when the rotor is declared settled,
before the encoder is zeroed. It compares the phase currents of the settled run with the ones the vector must
drive (REQ-PRO-005).

The vector puts `d·Vdc/2` across phase a and `−d·Vdc/4` across phases b and c, with `d` the test voltage as a
fraction of the bus. With the rotor at rest there is no back-EMF, so the currents settle at
`ia = d·Vdc/(2R)` and `ib = ic = −ia/2`. The service averages each phase over the settled run. It uses the
samples it already takes at the sampling frequency, never the 20 kHz ones. It declares a phase missing when:

- **phase a** carries less than a quarter of the expected `|ia|`. Phase a is the only one driven against the
  other two together, so this also catches a fully disconnected motor, where nothing flows;
- **phase b or c** carries less than a quarter of half of the expected `|ia|`. An open b or c leaves a and the
  other one in series, so the open phase reads zero while a still carries most of its current.

A quarter leaves room for what the model ignores. Dead time and switch drop take a few percent of the
alignment voltage, and the calibrated resistance can be off by more than that. A missing phase reads a few
noise samples' worth of current, far below it.

The current does not reach `ia` at once: it rises as `1 − e^(−t/τ)` with `τ = L/R` from the moment the vector is
applied. A rotor that already rests at electrical zero settles after the first few samples, before a slow winding
has carried much current. So the expected currents are scaled by the mean of `1 − e^(−t/τ)` over the settled
samples' own times. On the reference motors τ is under 1.2 ms and the scale is close to 1. The accepted ranges
allow τ up to about 10⁴ s (L up to 1 H, R down to 0.1 mΩ). This is far beyond any hold the alignment can afford.
So waiting for the current to settle instead would turn a slow winding into a failed alignment. A slow winding with
no current flowing still reads below a quarter of its scaled expectation.

With the follow step (Encoder Follow Check), the second hold does not start from zero. Each phase moves from the
current the first hold left to the current of the alignment angle, and the expectation follows that path. Phase b
reverses on the way. While its expected mean is under a quarter of its final share, a transition cannot be told
from an open phase. So b is not judged. Phases a and c, which keep their sign, are still judged.

The settled samples are taken in the control interrupt, but nothing is decided there. The interrupt stops
taking samples, records which samples were settled and hands the run to the event loop. The vector keeps
holding the rotor until the event loop has judged the currents and, on success, read and zeroed the encoder.
Only then is the inverter stopped. An abort between the hand-over and the decision stops the inverter and
discards the run without zeroing the encoder.

Without a resistance or a bus voltage the expected current is unknown. The check then falls back to the
ratio between the phases. Phases b and c must each carry at least a quarter of half of the measured `|ia|`. That
still catches an open b or c, but not a disconnected motor or an open a, which leave nothing to compare
against. Every caller in this firmware passes both.

The check needs all three phases measured. The alignment duties are 60 % and 45 % at the default 20 % vector.
They stay below the readable duty of every target, so no phase is rebuilt from the other two.

A missing phase ends the run like a timeout — inverter stopped, encoder untouched, completion empty — and the
service records why. The last failure answers *phase missing* after such a run, until the next alignment request.
This lets the caller enter Fault with *phase missing* instead of a generic calibration failure.

### Encoder Follow Check

An encoder can stop reporting, for example when it is stuck from boot, unplugged, or has a broken channel. Such
an encoder reads as a rotor that settled at once. The alignment would then zero it, and the drive would enable
against a reading that means nothing (REQ-CMS-001). With a non-zero follow step the service therefore aligns in
two holds:

1. **First hold.** The vector is applied at the follow step, a quarter electrical turn ahead of the alignment
   angle, and settlement is detected as above. The settled reading is recorded; nothing is zeroed.
2. **Second hold.** The vector moves to the alignment angle within the sampling interrupt, without stopping the
   inverter. The rotor turns back by the follow step in electrical angle, which is the follow step divided by
   the pole pairs mechanically. Settlement is not counted during the follow dwell. So a heavy rotor that has not
   yet started to move cannot be declared settled where it stood. Each hold has its own max samples.

When the second hold settles, the sampling interrupt hands the run to the event loop as without the follow step.
There the phase-continuity check runs on the second hold's currents, which are the currents of the alignment
angle. A missing phase is reported first, because a disconnected motor or an open phase also leaves the rotor
where it is. Then the travel between the two settled readings is compared with the expected one. The encoder followed when
the travel is in the direction the field turned and between half and one and a half times the expected angle.
Otherwise the run ends like a missing phase, with *encoder not following* recorded, which the caller reports as
*encoder loss*. The encoder is zeroed only after both checks pass, at the second hold, exactly as without the
follow step.

The bounds leave room for what moves a settled rotor off its electrical position. Friction and cogging hold it
a few electrical degrees short, and both holds settle within the same threshold. A quarter turn keeps the
second hold far from the unstable position half a turn away, so the rotor always turns the short way. A stuck
encoder travels nothing. An encoder wired with the wrong count direction travels the right angle the wrong way.
An encoder set to the wrong resolution or pole-pair count travels the wrong angle.

A rotor that cannot move — clamped, or held by more friction than the vector's torque — is reported the same
way. From the encoder alone the drive cannot tell it from an encoder that does not report. For the same reason,
the fault supervisor classifies a rotor locked before it moved as *encoder loss* (`fault-supervision.md`).

The first hold adds one settlement to every alignment. This takes tens of milliseconds for a free rotor, and up
to the first hold's own max samples for a slow one.

### State Machine

The service follows a five-state machine:

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Aligning : alignment requested
    Aligning --> Aligning : first hold settled\n(vector moves to the alignment angle)
    Aligning --> Judging : final hold settled\n(sampling interrupt hands the settled samples over; the vector keeps holding)
    Judging --> Idle : event loop judges the currents and the follow\n(zeroes the encoder if both pass, stops the inverter, completion delivered)
    Judging --> Idle : abort\n(inverter stopped, encoder untouched, completion dropped)
    Aligning --> Delivering : max samples exceeded or over-current\n(sampling interrupt stops the inverter)
    Aligning --> Idle : step timeout\n(inverter stopped, completes with no offset)
    Delivering --> Idle : event loop completes\nwith the offset or with none
    Aligning --> Idle : abort\n(inverter stopped, completion dropped)
    Delivering --> Idle : abort\n(completion dropped)
```

While in the **Aligning** state, any further alignment request is silently ignored — the service will not restart or nest. Only one alignment may be in progress at any time. Transitions out of the **Aligning** state always stop the inverter before delivering the completion. So the caller receives control only after the hardware is in a safe, stopped state.

### Completion Callback Semantics

The completion carries an optional offset in radians:

- An offset: alignment succeeded. The value is the encoder offset in mechanical radians, already applied to the encoder by the service.
- No offset: alignment failed — a timeout, an over-current, a missing phase or an encoder that did not follow the field. The caller must not use any offset. The last failure tells the last two apart, and only a timeout is worth retrying.

The completion fires exactly once per alignment request, from the event loop and never from the sampling interrupt. The service clears the completion when it fires, so that a stale reference cannot cause a second invocation.

---

## Interfaces

### Provided

| Interface    | Purpose                                                                                    | Contract                                                         |
|--------------|--------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| Force align  | Starts the alignment with the pole pairs and the configuration                             | Ignored if already aligning; completes once, from the event loop |
| Last failure | Why the last run failed: none, a missing phase or an encoder that did not follow the field | Valid from the completion until the next alignment request       |

The force-align completion reports the calibrated encoder offset, or a failure. The inverter has stopped before the completion fires.

### Required

| Interface            | Purpose                                                                         | Contract                                             |
|----------------------|---------------------------------------------------------------------------------|------------------------------------------------------|
| Three-phase inverter | Open-loop voltage during alignment; its interrupt paces the settlement detector | No other controller claims it during the alignment   |
| Encoder              | Rotor mechanical angle at each sampling tick, for settlement and the offset     | Initialised and tracking position before the request |
