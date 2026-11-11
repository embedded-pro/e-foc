---
title: "Virtual Motor App Design"
type: design
status: draft
version: 0.9.0
component: "tool-virtual-motor"
date: 2026-11-10
---

| Field     | Value                    |
|-----------|--------------------------|
| Title     | Virtual Motor App Design |
| Type      | design                   |
| Status    | draft                    |
| Version   | 0.9.0                    |
| Component | tool-virtual-motor       |
| Date      | 2026-11-10               |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
>
> **Diagrams**: All visuals must be Mermaid or ASCII art. External image references are **not allowed**.

---

## Responsibilities

**Is responsible for:**
- Starting, pausing, power-cycling and powering off virtual motors (`virtual-motor.md`), each a CAN
  node on a virtual bus, and restoring the set of motors on the next start
- Hosting the virtual CAN hub, and showing which buses exist and who is on them
- Showing, per motor, what bench instruments would show: phase currents, leg voltages and the
  modulation hexagon, the rotor and the current and voltage space vectors turning, the encoder against
  the true angle, speed and torque, supply and winding temperature, the board LEDs and the debug log
- Injecting load and disturbance torque and sensor noise while a motor runs, and making every active
  change visible and removable in one action
- Shipping inside the e-foc CAN Commander installers, as "e-foc Virtual Motor"

**Is NOT responsible for:**
- Commanding the drive. Start, stop, modes, setpoints, calibration and settings are the CAN
  Commander's, over the virtual bus, as for a real board; this app sends nothing on the bus
- Showing the drive's internal state. What the drive reports (state, fault, its measured Id and Iq,
  identified parameters) is read in the CAN Commander
- Recording or reporting test verdicts

---

## Component Details

### Part A — logic, support and presentation layers

As in the CAN Commander (`tool-can-commander.md` Part A), everything but widget layout is testable
without a display:

- **Engine client** — starts and stops virtual motor processes, holds each one's plant port, and turns
  the port's stream and captures into per-signal sample buffers. The app listens for plant ports on one
  loopback port it picks itself; each motor runs as an `e-foc-virtual-motor` process, installed next to
  the app, in a folder of its own under the user's application data (`<bus>-<node>`), which keeps its
  memory file. It joins the app's hub and connects to the app's plant port.
- **Hub** — the virtual CAN hub, run in this process while the app is open. When another hub already
  holds the port, the app's motors join that one and the header says so.
- **Logic** — the motor list and its persistence, the active-disturbance list per motor, capture
  requests and trigger settings, profile files.
- **Presentation** — the manager window and one window per motor. Widgets only display.

### Part B — the manager window

```text
+----------------------------------------------------------------------------------+
|  e-foc Virtual Motor                                   [ Open CAN Commander ]    |
|  Bus: default   ● hub running on port 29536                                       |
+----------------------------------------------------------------------------------+
|  Node | Plant    | State    | Real time | Disturbances | LEDs      |             |
|  1    | Teknic   | Running  | 1.00      | 2 active     | ● ○ ○     | [Open]      |
|  2    | Anaheim  | Paused   | —         | none         | ○ ● ○     | [Open]      |
|  3    | Teknic   | Crashed  | —         | —            | —         | [Log]       |
+----------------------------------------------------------------------------------+
|  [ + Add Motor ]  [ Power Off ]  [ Power Cycle ]  [ Pause ]  [ Clear Disturbances ] |
+----------------------------------------------------------------------------------+
```

- **Add Motor** asks for the node, the plant (Teknic, Anaheim, or a description file, which is where
  non-idealities and wiring faults are set) and how it starts: keep its memory or factory-fresh. Starting
  calibrated for the plant is offered once a virtual motor can (`virtual-motor.md` Open Question 4). A
  node already in the list is refused with the reason. It also asks for the current-sense offset of each
  phase, the offset a board's conditioning circuit adds to the measured currents, zero by default: it
  holds for the motor's life, is kept with the motor and shown with its parameters, and the firmware's
  offset calibration is expected to remove it.
- **State** is the process's (`virtual-motor.md` Part H), never what the user last clicked: *Starting…*
  until the motor introduces itself on the plant port, then *Running* or *Paused* as its status says;
  *Restarting* while a motor that reset reconnects; *Stopped* once powered off; *Crashed* when its process
  ends unasked. A crashed motor keeps its row with its last log lines one click away; the row explains that
  the CAN Commander will report it offline.
- A motor that introduces itself with another plant port protocol version (`virtual-motor.md` Part F)
  is powered off, and its log names both versions: the app and the virtual motor program it starts
  ship together.
- **Real time** shows the real-time factor; below 0.98 it is a warning with the overrun count in the
  tooltip, because the drive is then running late.
- **Open CAN Commander** starts the CAN Commander installed next to the app; its Interface box lists
  `virtual:default` while the hub runs (`tool-can-commander.md` Part N).
- The motor list, with each motor's node, plant and start mode, is restored on the next start; restored
  motors start again, keeping their memory.
- The bus row offers drop probability, added latency and taking a node off the bus (`virtual-motor.md`
  Part G), each listed under *Disturbances* while active.
- Closing the app powers every motor off, after a confirmation that names the motors and says the CAN
  Commander will see them go offline.
- **Eight motors.** A bus carries at most eight, the number of motors the CAN Commander follows. With
  eight in the list, Add Motor is disabled and its tooltip says why; a restored list longer than that
  starts its first eight and reports the rest.
- **Issue log.** Every motor added, removed, reset or crashed, every refusal, and every start-up
  failure (virtual motor program missing, plant port unavailable) goes to a persistent issue log: a
  rotating file in the per-user data folder, in the CAN Commander's format, with timestamps in UTC.
- **Help.** *Report Issue…* opens a pre-filled issue in the browser, with the app's version, the
  operating system and the last lines of the issue log, for the user to review before sending; nothing
  is sent on its own. *Open Log Folder* opens the issue log's folder. *About e-foc Virtual Motor* names
  the version, the plant port protocol, the Qt version, the log folder and the licence.

### Part C — the motor window

One window per motor, titled with node and plant, and marked **VIRTUAL** in its header, so it is never
mistaken for a bench instrument on real hardware.

```text
+------------------------------------------------------------------------------------------+
| VIRTUAL  Node 1 · Teknic M-2310P   Running  t = 00:01:42.315  RT 1.00   LEDs ● ○ ○  Prot: ok |
| [Pause] [Step 1 period] [Step 1 ms] [Power Cycle] [Power Off]  Disturbances: 2 [Clear All] |
+-----------------------------------------------------------+------------------------------+
| Tabs: Currents | Voltages | Mechanics | Supply | Modulation | Rotor | Encoder | Log        |
|                                                           |  Torque                      |
|   (selected view)                                         |    load, external            |
|                                                           |  Noise                       |
|                                                           |    current, encoder          |
|                                                           |  Profiles                    |
|                                                           |  Motor (read-only)           |
|                                                           |                              |
+-----------------------------------------------------------+------------------------------+
| Capture: trigger [Ia ▾] [rising ▾] level [ 2.00 A ]  pre 25 %  length 4096  [Arm] [Now]    |
+------------------------------------------------------------------------------------------+
```

The header is always in view: motor state, virtual time, real-time factor, the three LEDs as the board
would light them (with their meaning in text: operational, warning, failure), the board protection
state, and the number of active disturbances with **Clear All** beside it.

| View       | Shows                                                                                                                         |
|------------|-------------------------------------------------------------------------------------------------------------------------------|
| Currents   | True phase currents a, b, c with the measured ones overlaid, so the sensor's noise and bias are visible; true Id and Iq below |
| Voltages   | Duties and leg voltages a, b, c; stator voltage α and β                                                                       |
| Modulation | The hexagon of Part J with the stator voltage vector and its trail, beside the current in the d-q plane                       |
| Rotor      | The animated motor of Part E                                                                                                  |
| Encoder    | The monitor of Part F                                                                                                         |
| Mechanics  | Speed, true angle, electrical torque against load and external torque                                                         |
| Supply     | Bus voltage and current, winding temperature                                                                                  |
| Log        | The debug log, timestamped in virtual time, bounded, with filter, Clear and Save                                              |

### Part D — scopes

Every trace view is a scope on the continuous stream (`virtual-motor.md` Part F), drawn as the
minimum-maximum envelope of each 2 kHz window, so ripple and noise are seen at a glance without a full
rate stream.

| View      | Panels, top to bottom, each with its own vertical range and unit                              |
|-----------|-----------------------------------------------------------------------------------------------|
| Currents  | Phase currents a, b, c with the measured ones overlaid (A); true d and q currents (A)         |
| Voltages  | Duties a, b, c (1); leg voltages a, b, c (V); stator voltage α and β (V)                      |
| Mechanics | Rotor speed (rad/s); true and encoder angle (rad); electrical, load and external torque (N·m) |
| Supply    | Bus voltage (V); bus current (A); winding temperature (°C)                                    |

- **Stream on demand.** A motor's window asks for the stream while it is open and shows a trace view,
  and turns it off when it is closed, minimised or shows the log, so a motor nobody watches samples
  nothing. After a motor restarts the window asks again.
- **History.** The app keeps the last 10 s of windows per motor. The time axis shows the chosen span,
  10 ms to 10 s, ending at the newest window and labelled in time before it. A window the motor dropped
  breaks the trace instead of joining across the gap, and the toolbar counts the windows dropped since
  the stream was turned on. History starts over when the motor restarts, because its virtual time does.
- **Drawing.** Each panel fits its vertical range to what it shows, with its unit on the axis and a
  legend in the trace colours. When the span holds more windows than the panel has pixels, the
  windows that share a pixel column are drawn as one envelope, their lowest minimum to their highest
  maximum, so no peak is lost. Only the visible view repaints, at most 25 times a second.
- **Run and freeze.** **Freeze** keeps the traces on screen while the motor runs on, and the toolbar
  says *Frozen*; **Run** follows the newest window again.
- **Cursors.** In a frozen trace or a capture, a left click places cursor A and a right click cursor B;
  a double click removes both. Each panel reads, for each of its traces, the value at A and at B and
  their difference, and the toolbar reads both times, their difference and its inverse. A value in a
  frozen trace is the middle of its envelope at that time; in a capture it is the sample.
- **Captures.** The capture bar arms a full-rate capture: trigger (immediately, rising, falling, either,
  board protection, LEDs), the trigger signal and level in that signal's unit for an edge, the LEDs
  for an LED trigger, the pre-trigger share and the length in control periods. Its state reads *Armed…*
  until the trigger fires, then *Triggered*, *Receiving n of m* while the chunks arrive, and
  *Complete*; **Cancel** stops an armed or recording capture, and arming again replaces it. A complete
  capture opens in the view of its trigger signal, or the current view when the trigger has none, at
  every control period, with the time axis at zero on the trigger; **Back to Live** returns to the
  stream. A motor that restarts cancels its capture.
- **Pause and step.** While the motor is paused the trace stays at the paused instant. **Step 1
  period** and **Step 1 ms** advance it; the trace moves when a step completes a 10-period window,
  and a capture armed before stepping records every period stepped through.
- **Export.** **Save CSV…** writes the capture shown: one row per control period, the first columns
  the time from the trigger in seconds and the control period, then every Data Model signal in its
  order, each column headed with its name and unit.

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Armed: Arm
    Armed --> Triggered: capture event triggered
    Triggered --> Receiving: first chunk
    Receiving --> Complete: every period received
    Armed --> Cancelled: Cancel, or the motor restarts
    Triggered --> Cancelled: Cancel, or the motor restarts
    Receiving --> Cancelled: the motor restarts
    Complete --> Armed: Arm
    Cancelled --> Armed: Arm
```

### Part E — the rotor

A cross-section of the motor, drawn at the true mechanical angle:

- The stator with its three phase axes, each winding shaded by the sign and size of its current: warm for
  a current into the winding, cool for one out of it, stronger as it nears the largest current shown.
- The rotor with the pole pairs of its plant description, alternating north and south, the d axis marked.
- The current space vector (the amplitude-invariant Clarke of the true phase currents) and the stator
  voltage vector, drawn from the centre in the stationary frame, each scaled to its own largest magnitude
  and labelled with it, with the d and q axes turning with the rotor, so the angle between current and q
  shows field weakening or a misalignment.
- The encoder's angle as a second marker on the rotor rim, which separates from the true one when the
  encoder is noisy, biased or stuck.

The rotor shows one instant: the newest window's last control period while live, the newest frozen one
when frozen, and the trigger period of a capture. It repaints at most 25 times a second and shows the
true speed beside it. Above the speed at which the rotor would turn more than a quarter turn between
frames it draws the rotor fixed, d axis up, with the vectors in the rotor frame, and says so, rather
than drawing an aliased rotation.

### Part F — the encoder monitor

Over the chosen span, from each window's last control period while live and from every period of a
capture:

- The difference between the true angle and the encoder's angle, in electrical degrees and wrapped to a
  half turn, over time.
- A histogram of the difference, with its mean (bias) and standard deviation (noise).
- The encoder's counts per revolution and its resolution in electrical degrees from the plant
  description, or that it is ideal when the description gives no counts, and a *stuck* indicator when
  the reported angle has not changed over the last 10 ms while the true angle moved by more than one
  count, or by a thousandth of a radian for an ideal encoder.

The index pulse is not in the Data Model, so the monitor does not show it.

### Part G — torque and noise

The side panel holds the changes of `virtual-motor.md` Part E: **Torque** (load torque, and external
torque as a constant, a step, a ramp or a sine) and **Noise** (current noise, encoder noise).
Below them, the motor's parameters are shown read-only from its plant description, with a note that
another motor means another description and a restart. Values are typed or
dragged but **apply only on Apply** or Enter, never while typing; each group shows what is in force.
Every active change appears in the header count and in a list with its own remove button; **Clear All**
returns the plant to its description.

**Profiles** (`virtual-motor.md` Part E) are written, loaded, saved, started and stopped from the
panel's Profile group. The profile is plain text in an editor there, so a profile is written by hand or
loaded from a file and changed before it starts. **Start** reads it and sends it to the motor; a line it
cannot read is reported with its number and nothing is sent, and a profile the motor refuses is reported
as a refused change is. The group reads what the motor reports: *Armed, waiting for the trigger*;
*Running* with the changes applied, the time since the trigger and the next change; *Finished*, its changes
staying in force; or *Stopped*, the plant back to its description. **Stop** stops it.

A profile file holds one statement per line; blank lines and lines starting with `#` are skipped:

| Statement                                         | Meaning                                                                                                                     |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `trigger immediately`                             | Start at once; the default when no trigger is given                                                                         |
| `trigger rising` / `falling` / `either` *n level* | Start when Data Model signal *n* (1 to 25, in the order of `virtual-motor.md`'s Data Model) crosses *level* in its own unit |
| `trigger protection`                              | Start when the board protection asserts                                                                                     |
| `trigger leds` *names*                            | Start when one of the named LEDs lights: `operational`, `warning`, `failure`, separated by commas                           |
| `at` *s* `load` *N·m*                             | Set the load torque *s* seconds after the trigger                                                                           |
| `at` *s* `external none`                          | Remove the external torque                                                                                                  |
| `at` *s* `external constant` *N·m*                | A constant external torque                                                                                                  |
| `at` *s* `external step` *from to delay*          | A step, the delay in seconds                                                                                                |
| `at` *s* `external ramp` *from to duration*       | A ramp, the duration in seconds                                                                                             |
| `at` *s* `external sine` *offset amplitude hz*    | A sine                                                                                                                      |
| `at` *s* `current-noise` *A*                      | Set the current noise                                                                                                       |
| `at` *s* `encoder-noise` *rad*                    | Set the encoder noise                                                                                                       |

Times count from the trigger, are rounded to the control period, and never go back; a profile holds up
to 1,024 changes. **Save** writes the editor's text as it stands, so a saved file loads back unchanged.

### Part H — the CAN Commander side

The CAN Commander needs no knowledge of the plant. A node whose board kind is *virtual* is marked
**Virtual** in its tab and overview chip, so a bench engineer never confuses it with hardware
(`tool-can-commander.md` Part N). Its Motor menu offers **Launch Virtual Motor…**, which starts this app.

### Part I — installers

The app and the virtual motor program ship in the CAN Commander's Windows and Debian installers, with
their own start-menu entry and icon. The installers' asset names stay unchanged, so the update check
finds them as before.

- **Debian package.** `e-foc-virtual-motor-app` and `e-foc-virtual-motor` install beside
  `e-foc-can-commander`, so the app finds the virtual motor program and the CAN Commander's Launch
  Virtual Motor… finds the app. The menu entry *e-foc Virtual Motor* carries its own icon: the CAN
  Commander's motor inside a dashed ring, which also marks the app's windows. The release's install test
  installs the package, checks both entries and icons, asks the virtual motor program for its usage,
  runs the app, and removes the package.
- **Windows installer.** `e-foc-virtual-motor-app.exe` and `e-foc-virtual-motor.exe` install beside
  `e-foc-can-commander.exe` and share its Qt runtime, and the Start menu gains *e-foc Virtual Motor*
  beside *e-foc CAN Commander*. The app's executable carries the dashed-ring icon and the release's
  version. The portable archive holds the same three programs. The release's install test installs
  silently, checks both programs and the shortcut, asks the virtual motor program for its usage, runs a
  virtual motor and checks that ending it leaves no motor process behind, runs the app, and uninstalls.

### Part J — modulation

The space-vector hexagon of the shared tool UI library, drawn for the bus voltage of the plant: its six
switching vectors at two thirds of the bus voltage, the circle of linear modulation at the bus voltage
over √3, the phase leg voltages a, b and c as phasors, and their resultant, the stator voltage vector,
with its sector shaded. Over it, the trail of the stator voltage vector across the chosen span, fading
with age, from each window's last control period while live and from every period of a capture. A
vector beyond the linear circle reads *Overmodulation*.

Beside the hexagon, the true current in the d-q plane: its trail across the same span and its newest
point, on axes symmetric about zero and scaled to the largest current shown, so a drive holding Id at
zero draws a vertical line and noise or a misalignment widens or tilts it.

---

## Interfaces

### Provided

| Interface        | Purpose                                     | Contract                                                    |
|------------------|---------------------------------------------|-------------------------------------------------------------|
| Virtual CAN hub  | Buses for virtual motors and CAN Commanders | `virtual-motor.md` Part G, while the app is open            |
| Persisted motors | The rig reopens with its motors             | Node, plant, start mode and memory file per motor, per user |

### Required

| Interface        | Purpose                                       | Contract                                         |
|------------------|-----------------------------------------------|--------------------------------------------------|
| Virtual motor    | The motors themselves                         | `virtual-motor.md`: start options, plant port    |
| Theme and scopes | Look, status colours, scope and hexagon views | The shared tool UI library, as the CAN Commander |

---

## Constraints & Limitations

| Constraint    | Value / Description                                                   |
|---------------|-----------------------------------------------------------------------|
| Motors        | Eight per bus                                                         |
| Repaint rate  | At most 25 Hz per view; only the visible view of each window repaints |
| Log           | Bounded per motor; the full log stays in the motor's folder           |
| Trace history | The last 10 s of stream windows per motor                             |
| Capture       | One per motor, up to 16,384 control periods, kept until replaced      |
| Bus commands  | None: the app never sends a CAN frame                                 |

---

## Open Questions

| # | Question                                                           | Options                                           | Status           |
|---|--------------------------------------------------------------------|---------------------------------------------------|------------------|
| 1 | Should the CAN Commander show a virtual node's plant views itself? | No, this app only; or a read-only plant tab later | open, leaning no |
