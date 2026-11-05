---
title: "Fault Supervision Design"
type: design
status: draft
version: 0.5.0
component: "supervision"
date: 2026-11-05
---

| Field     | Value                    |
|-----------|--------------------------|
| Title     | Fault Supervision Design |
| Type      | design                   |
| Status    | draft                    |
| Version   | 0.5.0                    |
| Component | supervision              |
| Date      | 2026-11-05               |

---

## Responsibilities

**Is responsible for:**
- Watching an enabled drive from the event loop for effort without motion: torque current that asks the
  rotor to move while the encoder reading stays put (REQ-SM-028).
- Telling the two causes it can tell apart: a rotor an outside force pushed against the drive and then
  held (`stall`), and everything else — an encoder that stopped reporting, or a rotor locked before it
  ever moved (`encoderLoss`).
- Raising the fault through the platform fault notifier, as a board protection trip is raised.
- Using the drive-wide supervision settings of the configuration record from its next evaluation, and
  leaving stall reporting off when the operator turned it off (REQ-SM-033).
- Watching an enabled drive for a current it demands but cannot make flow, and raising `phaseMissing` when the
  motor is disconnected while it runs (REQ-EH-015, Part E).
- Watching an enabled drive, in every control mode, for an encoder reading that stops dead faster than the rotor
  could physically stop, and raising `encoderLoss` (REQ-SM-028, Part F). This is the watch that covers torque
  mode, where the drive demands no motion for Parts A to C to judge.
- Watching an enabled drive, in every control mode, for a rotor that turns faster than the overspeed limit allows,
  and raising `overspeed` (REQ-PRT-003, Part G).
- Watching an enabled position drive for a position error that stays beyond the operator's following-error
  window for the following-error time, and raising `followingError` (REQ-PRT-004, Part H).
- Keeping a thermal model of the motor's winding from its current and folding the drive's current limit back
  from the peak to the continuous rating when the motor is overloaded (REQ-PRT-001, Part I).
- Watching an enabled position drive for a rotor carried more than four encoder counts past the software
  position limits, and raising `positionLimit` (REQ-POS-016, Part J).

**Is NOT responsible for:**
- Checking anything in the 20 kHz control interrupt. Per-sample plausibility checks are forbidden there
  (REQ-PERF-003); sensor and power-stage failures that need a faster answer belong to board protection.
- Telling a frozen encoder from a rotor locked before it moved. Both look the same from the drive — current
  flows, the error does not close, the reading does not change — so both are reported as `encoderLoss`.
- Storing or range-checking settings on the wire. The CAN settings bridge validates a write against the
  wire contract and stores it in the configuration record; the supervisor only reads the record.
- Detecting a single open phase while the motor runs. Two phases still carry current and turn the rotor, so
  the shortfall Part E watches for does not appear; an open phase is caught at the next alignment
  (`service-alignment.md`, phase continuity check), which runs at every power-up.
- Catching an encoder that freezes while the rotor turns slowly and the drive demands no motion, as torque mode
  at low speed. Part F needs a speed the rotor could not have shed, and Parts A to C need a demand; an encoder
  that stops below that speed in torque mode is found at the next alignment (`service-alignment.md`, encoder
  follow check).
- Limiting the speed. The torque cascade folds its current against the overspeed limit (`foc-torque.md`) and
  the control-mode state machine holds a speed setpoint at it; Part G only faults a rotor that passed both.
- Measuring the winding temperature. Part I models the heat the current puts in from the ratings alone; a
  winding that heats from outside, or a rating set too high, is not seen.
- Protecting the motor from heat while a stall is not reported, unless the continuous rating of Part I is set. With stall reporting off, a held rotor can
  carry up to the current limit; only the thermal protection of the board watches that.

---

## Component Details

### Part A — Pushing for motion

Every 10 ms the supervisor reads what the active drive observes of its own motion — the measured torque
current, the speed it demands and the position error it has not closed — and the encoder reading. The
drive *pushes for motion* when two things hold together:

- the torque current is at least the configured fraction of the drive's current limit, and
- the drive demands motion: a position error of at least the configured threshold or, without one, a
  demanded speed of at least the configured threshold.

The *direction* of the push is the sign of that position error or, without one, of the demanded speed. A
position law that commands the current demands no speed, so the stale speed reference it leaves in the
speed loop is not a demand. A drive holding its position against a load carries current but has no error
to close, so it does not push for motion.

### Part B — The still window

While the drive pushes for motion, the supervisor anchors the encoder reading and counts how long it
stays within the configured excursion of that anchor. Travel beyond the excursion moves the anchor and
starts the count again; noise within it does not. When the count reaches the configured window the
supervisor raises one fault and stays silent until the drive leaves `Enabled`. Any evaluation in which the
drive does not push for motion clears the count.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Watching : Enabled and pushing for motion
    Watching --> Watching : travel beyond the excursion (new anchor)
    Watching --> Idle : not pushing for motion
    Watching --> Reported : still for the whole window, cause reported
    Watching --> Watching : still for the whole window, stall with stall reporting off
    Reported --> Idle : drive leaves Enabled
    Idle --> Idle : not Enabled
```

### Part C — Telling a stall from an encoder loss

An encoder that has stopped reporting cannot show the rotor moving against the drive. A rotor that an
outside force pushes away from where the drive is taking it, and then holds, can — the encoder saw the
push. The supervisor therefore remembers, for the latest travel of at least the configured excursion it
saw while the drive was enabled, whether that travel went against the direction the drive was pushing:

| Latest travel                              | Evidence    | Fault after a still window |
|--------------------------------------------|-------------|----------------------------|
| Against the push, while pushing for motion | overpowered | `stall`                    |
| Along the push, while pushing for motion   | none        | `encoderLoss`              |
| While not pushing for motion               | none        | `encoderLoss`              |
| None since the drive was enabled           | none        | `encoderLoss`              |

Travel is measured from its own anchor, independent of the still window's, and every evaluation takes
part, so a push that crosses the thresholds within one evaluation period is still seen as one. The
evidence is cleared whenever the drive leaves `Enabled`.

Braking does not count as being overpowered: a drive that brakes towards a position setpoint carries
current against the travel, but the travel still goes the way the position error points. A speed reversal
is the one case the rule reads wrong: while the rotor still turns the old way, the travel goes against the
new demand, so an encoder that freezes during a reversal is reported as a stall. The fault is raised either
way unless stall reporting is off.

### Part D — Settings

The supervisor keeps no settings of its own. It reads the supervision fields of the configuration record
(`service-nvm.md`) at each evaluation, so a value written over CAN (`service-can.md`, contract 2.4) is in
force from the next one, in any state. The fields are one set for the whole drive; no control mode has its
own.

| Setting          | Default  | Meaning                                                         |
|------------------|----------|-----------------------------------------------------------------|
| Window           | 0.1 s    | How long the reading has to stay still while the drive pushes   |
| Current fraction | 0.05     | Torque current, as a fraction of the current limit, that counts |
| Demanded speed   | 2 rad/s  | Speed reference that counts as a demand                         |
| Position error   | 0.2 rad  | Mechanical position error that counts as a demand               |
| Excursion        | 0.01 rad | Travel within which the reading counts as still                 |
| Stall reporting  | on       | Whether a stall is raised; encoder loss is always raised        |

A field outside its range — possible only if a record was written by a firmware with wider ranges and the
layout was not bumped — is replaced by its default when the record is loaded at boot (`service-nvm.md`), so
the supervisor never runs on a value the wire would refuse and a query answers the value in force. With stall reporting off, a still window that ends with the overpowered evidence starts
again instead of raising anything; the supervisor keeps watching, and an encoder that stops reporting after
the rotor is released is raised as `encoderLoss`.

### Part E — Phase loss while running

A second watch, the phase-loss monitor, runs on the same 10 ms period and reports `phaseMissing` through the
same notifier. It looks for a current the drive demands but cannot make flow: an evaluation counts when

- the drive is `Enabled`;
- the torque current it demands is at least 10 % of the current limit;
- the torque current it measures is below 20 % of that demand; and
- the rotor turns slower than 10 rad/s.

The span starts at the first evaluation that counts. When the evaluations have counted without a break for
the span, the monitor raises the fault once and stays silent until the drive leaves `Enabled`. An evaluation
that does not count ends the span; the next one that counts starts a new one.

The span is 50 ms, or one current-loop time constant when the operator's current bandwidth makes that longer
(1/ω_c: 1 s at the 1 rad/s minimum). A healthy first-order loop passes a fifth of a step after 0.22 time
constants, so a slow loop is never mistaken for an open circuit while its current rises. The automatic
bandwidth is far above 20 rad/s and leaves the span at 50 ms.

The speed condition is what keeps a connected motor from tripping. At standstill there is no back-EMF and the
whole supply drives current through the winding resistance, many times the current limit for any motor the
drive is sized for, so a current loop that cannot reach a fifth of its demand for 50 ms is pushing into an
open circuit. At speed the back-EMF takes most of the voltage, and a speed loop asking for
more than the motor can reach legitimately demands the current limit while the measured current stays small.
The 10 rad/s bound leaves the back-EMF under 0.3 V on both reference motors.

A cable pulled while the rotor turns faster than that is reported once friction has slowed the rotor below
it. A single open phase is not reported: two phases still carry current and turn the rotor, so the shortfall
never appears. Both limits are recorded in REQ-EH-015.

The demanded current is what the active drive commands its current loop: the torque setpoint after the
current envelope in torque mode, the speed loop's output (or the current a position law commands directly)
in speed and position modes. The thresholds are fixed; the supervision settings of Part D do not apply.
The current bandwidth is read from the configuration record at every evaluation.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Counting : Enabled, demanding current it does not get, rotor slow
    Counting --> Idle : evaluation does not count
    Counting --> Reported : 50 ms counted, phaseMissing raised
    Reported --> Idle : drive leaves Enabled
```

### Part F — An encoder that stops dead

Parts A to C judge effort against motion, so they need the drive to demand motion. In torque mode it demands
none: a rotor held against its torque is the purpose of a tension or haptic drive, and still while pushing is
not a fault there. An encoder that freezes in torque mode is still visible, from kinematics alone. A turning
rotor cannot stop faster than the torques on it allow, while a frozen reading stops at once.

A third watch, the encoder-freeze monitor, reads the encoder every 1 ms on the event loop while the drive is
`Enabled`, in every mode. Between two readings it records the travel, and the speed that travel implies over
the longest the interval could have lasted: the timer advances in whole milliseconds, so a measured interval
of n ms lasted at most n + 1. A reading that moved less than half an encoder count is *still*.

When readings turn still right after an interval measured at a speed ω, the rotor must have shed ω within
that interval. Decelerating at the largest rate α it can reach, the speed it can have averaged over an
interval of at most T while ending slow enough to stay still is

  ω_max = α·T/2 + √(α·q)

with q one encoder count. √(α·q) is the speed from which it stops within half a count. An encoder that freezes
part-way through that interval leaves it only part of its travel, so the interval before it is judged as well:
measured at ω′ over at most T′, the rotor had T′/2 + T to shed it, and

  ω′_max = α·(T′/2 + T) + √(α·q)

If either speed exceeds its bound and is at least 20 rad/s, and the reading then stays still for 5 ms, the
monitor raises `encoderLoss` once and stays silent until the drive leaves `Enabled`. Any travel during the
5 ms starts the watch again.

α is twice the calibrated torque constant (`1.5·p·ψ`) times the current limit over the calibrated inertia.
The factor two allows for a load torque as large as the motor's own. When the calibration record has no
inertia or no flux linkage, as after a torque-mode calibration, α falls back to twice 2·10⁴ rad/s² per ampere
of current limit. That is about three times the torque-to-inertia ratio of either reference motor
(`documentation/theory/foc-plant-models.md`), so the fallback is looser than the truth for any motor that light or heavier.

| Drive                 | α (rad/s²) | ω_max at T = 2 ms |
|-----------------------|------------|-------------------|
| Teknic, 3 A board     | 32 600     | 40 rad/s          |
| Teknic, 20 A emulated | 217 600    | 236 rad/s         |
| Fallback, 3 A board   | 120 000    | 134 rad/s         |

Because the timer only bounds each interval to whole milliseconds, a measured speed can be half the real one.
On the emulated 20 A drive the monitor therefore needs a rotor faster than about 470 rad/s to catch a freeze
between two evaluations; the SIL scenario runs the bench plant, whose 3 A drive bounds α ten times lower.

The 20 rad/s floor is ten counts per millisecond on a 4000-count encoder, so one count of quantisation cannot
make a slow rotor look fast. The 5 ms confirmation lets a collision with an elastic stop show its rebound
before anything is raised. A rigid stop hit fast enough to stop within one interval without rebounding is
reported as `encoderLoss`. From the encoder that is indistinguishable from a freeze, and a fault is due
either way.

```mermaid
stateDiagram-v2
    [*] --> Moving
    Moving --> Moving : reading travelled, speed recorded
    Moving --> Suspect : reading still after a speed above ω_max
    Moving --> Resting : reading still after a speed the rotor could shed
    Suspect --> Reported : still for 5 ms, encoderLoss raised
    Suspect --> Moving : reading travelled
    Resting --> Moving : reading travelled
    Reported --> Moving : drive leaves Enabled
```

### Part G — Overspeed

A fourth watch, the overspeed monitor, reads the speed the active drive measures every 1 ms on the event loop
while the drive is `Enabled`, in every mode. An evaluation counts when the magnitude of that speed exceeds
110 % of the overspeed limit; five evaluations that count in a row raise `overspeed` once, and the monitor stays
silent until the drive leaves `Enabled`. An evaluation that does not count starts the count again.

The overspeed limit is one drive-wide setting of the configuration record (`service-nvm.md`), read at every
evaluation, so a value written over CAN (`service-can.md`, contract 2.9) is in force from the next one:

| Setting         | Default    | Range          | Meaning                                                             |
|-----------------|------------|----------------|---------------------------------------------------------------------|
| Overspeed limit | 1000 rad/s | 1 – 5000 rad/s | Mechanical speed the torque limiter folds against; faulted at 110 % |

The same limit bounds the speed twice before the monitor sees it: the torque cascade folds a current that pushes
the rotor towards it to zero (`foc-torque.md`, Speed Limiter), and the control-mode state machine holds a speed
setpoint beyond it at the limit (`state-machine.md`). A healthy drive therefore never reaches 110 %; the
monitor catches an outside torque that drives the rotor past the limit, a speed or position loop that overshoots
through a tuning or sensor fault, and a load that runs away in position mode.

The speed is the one the drive reports in its motion observation: the filtered per-sample speed in torque mode,
the outer loop's window speed in speed and position modes. Five 1 ms evaluations bound the reaction to
5–6 ms after the speed passes 110 %, inside the 10 ms of REQ-PRT-003, and keep a single noisy speed sample from
faulting the drive. The fault is raised through the same notifier as the other watches, so the drive cuts its
power stage and coasts, as for every other fault.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Counting : Enabled and |ω| > 1.1 · limit
    Counting --> Idle : |ω| ≤ 1.1 · limit
    Counting --> Reported : five evaluations in a row, overspeed raised
    Reported --> Idle : drive leaves Enabled
```

### Part H — Following error

A fifth watch, the following-error monitor, reads the position error the active drive observes every 1 ms on
the event loop while the drive is `Enabled`. It is the mechanical setpoint minus the measured angle, wrapped to
half a turn, the same error Part A judges; only position mode has one, the other modes report zero and are
never faulted. An evaluation counts when the magnitude of the error exceeds the following-error window. The span
starts at the first evaluation that counts; when the evaluations have counted without a break for the
following-error time, the monitor raises `followingError` once and stays silent until the drive leaves
`Enabled`. An evaluation that does not count ends the span.

| Setting                | Default | Range        | Meaning                                           |
|------------------------|---------|--------------|---------------------------------------------------|
| Following-error window | 0 (off) | 0 – 3.2 rad  | Position error that counts; 0 turns the check off |
| Following-error time   | 0.1 s   | 0.001 – 10 s | How long the error has to stay beyond the window  |

Both are drive-wide limits of the configuration record (`service-nvm.md`), read at every evaluation, so a value
written over CAN (`service-can.md`, contract 2.9) is in force from the next one.

The window is off by default because the position setpoint reaches the loop as a step: the error right after a
step is the step itself, and it stays beyond any window narrower than the step for as long as the loop needs to
cover the distance — about 145 ms for a 1.5 rad step under the position PID on the bench plant. An operator who
commands steps sets the window above the largest step, or the time above the time the largest step takes to
come inside the window; one who streams a smooth setpoint can set both tight. A rotor jammed while the drive
pushes for motion is reported as a stall by Parts A to C whether or not the window is set; the following error
adds a rotor that moves but does not keep up, such as a load the current limit cannot overcome.

The fault is raised through the same notifier as the other watches, so the drive cuts its power stage and
coasts, as for every other fault.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Counting : Enabled, window above zero and |error| > window
    Counting --> Idle : |error| ≤ window
    Counting --> Reported : beyond the window for the following-error time, followingError raised
    Reported --> Idle : drive leaves Enabled
```

### Part I — Motor overload

A motor is rated for a continuous current it can carry for ever and a peak current it can carry for a limited
time; the board's current limit is a third, flat bound. The overload model keeps the ratings apart. Every 1 ms
on the event loop, in every state, it updates a thermal state θ, in A², from the torque current the active drive
measures:

  dθ/dt = (i² − θ)/τ,  τ = t_peak / −ln(1 − i_cont²/i_peak²)

τ is chosen so that a cold motor (θ = 0) carrying the peak current reaches θ = i_cont² after exactly the peak
time. While θ stays below i_cont² the drive may carry the peak current; once θ reaches i_cont² the current limit
folds back to the continuous rating, and it is released to the peak again only once θ has fallen below half of
i_cont², so the limit does not chatter at the threshold. A motor that keeps carrying its continuous rating holds
θ at i_cont² and stays folded back, as it should.

| Setting            | Default         | Range      | Meaning                                                   |
|--------------------|-----------------|------------|-----------------------------------------------------------|
| Continuous current | 0 (off)         | 0 – 100 A  | Current the motor carries for ever; 0 turns the model off |
| Peak current       | 0 (board limit) | 0 – 100 A  | Current a cold motor may carry for the peak time          |
| Peak time          | 1 s             | 0.1 – 60 s | How long a cold motor may carry the peak current          |

Both ratings are capped at the board's current limit. A continuous rating at or above the peak leaves no overload
regime: the current is limited to the continuous rating flatly. With the continuous rating at zero the model is
off, θ is cleared and the board's limit is in force, as before.

The state is never reset by the lifecycle: a disabled drive measures no current, so θ decays with τ, and a
Disable followed by an Enable does not hand a hot motor its peak again. A reset of the controller does clear it;
the model then assumes a cold motor.

The limit reaches the cascades through the control-mode state machine, which keeps the latest one and hands it to
the active cascade whenever the model changes it and whenever a mode is activated. Each cascade bounds its q-axis
reference by it in the control interrupt, next to the board's limit (`foc-torque.md`, `foc-speed.md`). A speed or
position law whose output the fold-back cuts keeps integrating up to its own clamp at the board's limit, so when
the limit is released the current may rise to the peak at once; the law's anti-windup still bounds its state.
Each change of the limit is traced.

```mermaid
stateDiagram-v2
    [*] --> Peak
    Peak --> FoldedBack : θ ≥ i_cont², limit = continuous rating
    FoldedBack --> Peak : θ < 0.5 · i_cont², limit = peak rating
```

### Part J — Software position limit

The software position limits keep the reference and the way to it between the position minimum and maximum
(`foc-position.md`, Software Position Limits), but an outside force, or an overshoot, can still carry the rotor
past them. A sixth watch, the position-limit monitor, reads the encoder's mechanical angle every 1 ms on the event
loop while position mode is the active mode, the drive is `Enabled` and the limits are on. When the angle lies
more than four encoder counts outside the limits — 2π · 4 / resolution, 6.28 mrad with the default 4000-count
encoder — the monitor raises `positionLimit` (REQ-POS-016) once and stays silent until the drive leaves
`Enabled`. The four counts keep a rotor settled on a limit, whose reading dithers by a count, from faulting.

The angle is the one the loop works on, wrapped to [−π, π), so a rotor pushed past a limit and across the encoder
seam reads as far outside the other limit and still faults. The limits and the encoder resolution are read at
every evaluation, so a limit written over CAN is in force from the next one. Torque and speed mode are not
watched: their travel is not bounded by the limits. The fault is raised through the same notifier as the other
watches, so the drive cuts its power stage and coasts.

```mermaid
stateDiagram-v2
    [*] --> Watching
    Watching --> Reported : more than four counts outside the limits, positionLimit raised
    Reported --> Watching : drive leaves Enabled
```

---

## Interfaces

### Provided

| Interface     | Purpose                                                                                                                       | Contract                                                                               |
|---------------|-------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| Fault         | Reports a stall, an encoder loss, a missing phase, an overspeed, a following error or a position-limit violation to its owner | At most once per enable and watch; called from the event loop, never from an interrupt |
| Current limit | Hands the overload model's current limit to its owner (Part I)                                                                | Called from the event loop when the limit changes                                      |

### Required

| Interface        | Purpose                                                                                    | Contract                                                                     |
|------------------|--------------------------------------------------------------------------------------------|------------------------------------------------------------------------------|
| Active drive     | Lifecycle state and the motion the drive observes                                          | Read every evaluation; the observation is written by the control interrupts  |
| Encoder          | Mechanical angle                                                                           | Read every evaluation while the drive is enabled                             |
| Settings         | The configuration record's supervision fields and limits                                   | Read every evaluation                                                        |
| Repeating timer  | 10 ms evaluation period (1 ms for Parts F to I)                                            | Runs on the event loop                                                       |
| Kinematic limits | Current limit, encoder resolution, and the calibrated inertia, flux linkage and pole pairs | Read when a still reading follows travel; missing values select the fallback |
