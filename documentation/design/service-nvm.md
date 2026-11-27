---
title: "Service: Non-Volatile Memory"
type: design
status: draft
version: 0.17.2
component: service-nvm
date: 2026-11-27
---

| Field     | Value                        |
|-----------|------------------------------|
| Title     | Service: Non-Volatile Memory |
| Type      | design                       |
| Status    | draft                        |
| Version   | 0.17.2                       |
| Component | service-nvm                  |
| Date      | 2026-11-27                   |

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
- Persisting across power cycles, using the target's non-volatile memory:
  - motor calibration data (R, L, pole pairs, current offsets and noise, inertia, friction, cogging map, encoder
    index angle)
  - system configuration (CAN and start-up settings, telemetry rate, loop tunings and algorithm selections,
    fault-supervision settings)
  - the drive-wide limits of the system configuration:
    - overspeed, following error, speed ramps and command timeout
    - motor current ratings, target window and travel limits
    - temperature, bus voltage and quick-stop deceleration
- Persisting the fault history and the learned motor model, the inductance, inertia and friction online
  recalibration adopted (`online-recalibration.md`, Part F)
- Ensuring data integrity on every read through CRC-32 verification and on every write through read-back verification
- Rejecting a record written with another layout version as invalid, so it is never misread
- Distinguishing the calibration and configuration regions by per-region magic numbers, preventing cross-region misinterpretation of data
- Providing non-blocking, completion-based operations — no operation may block the calling context

---

## Component Details

### Logical Regions

The service manages four independent non-volatile memory regions. Each region holds exactly one record at a time — the latest write replaces any previous record in that address range.

| Region        | Magic        | Content              | Data size |
|---------------|--------------|----------------------|-----------|
| Calibration   | `0xCAFEF00D` | calibration record   | 100 bytes |
| Configuration | `0xDEADBEEF` | configuration record | 192 bytes |
| Fault history | `0xFA17C0DE` | fault history        | 112 bytes |
| Learned model | `0x1EA4ED0D` | learned model        | 26 bytes  |

The calibration record's layout is at version 6. Version 6 dropped four fields that nothing read:

- the q-axis inductance, always equal to the d-axis one
- the alignment angle and the encoder direction, since the rotor reference lives in working memory and the index
  angle
- a current-loop bandwidth calibration, only ever carried forward as 0

Version 6 also widened the current noise to two bytes, so a noise above 255 mA is stored instead of saturating. That
took the data from 112 to 100 bytes. A record written by an earlier firmware therefore reads as invalid, and the
motor has to be calibrated again. Version 5's encoder index angle (`service-index-search.md`) had taken it from 108
to 112 bytes. Version 4's cogging map (`service-cogging-compensation.md`) had taken it from 56 to 108. A record without an index angle
holds a NaN there. With its 9-byte header (magic, version and checksum), the 109-byte record fits the 128-byte
calibration region.

The configuration record's layout is at version 17. The circular-positioning toggle (`foc-position.md`, Multi-Turn
Position) took its data from 188 to 192 bytes: one byte for the toggle and three reserved. A record written at
version 16 therefore reads as a version mismatch. The configuration then returns to its defaults, which select
linear position mode.

Version 16's quick-stop deceleration (`state-machine.md`, Quick Stop) took its data from 184 to 188 bytes. A record
written at version 15 therefore reads as a version mismatch, and the configuration returns to its defaults.

Version 15's motion profile four limits (`foc-position.md`, Motion Profile) took its data from 168 to 184 bytes,
with no padding. A record written at version 14 therefore reads as a version mismatch. The configuration then
returns to its defaults, which leave the profile off.

Version 14's homing requirement (`foc-position.md`, Homing and the Position Reference) took the last reserved byte. A
record written at version 13 therefore reads as a version mismatch, and the configuration returns to its defaults.
The record keeps its 168 bytes.

Version 13's recalibration mode (`online-recalibration.md`) had taken one of the two reserved bytes. The earlier
versions changed the data size as follows:

- version 12, from 160 to 168 bytes, for the bus-voltage minimum and maximum (`fault-supervision.md` Part L)
- version 11, from 152 to 160 bytes, for the temperature limit and margin (Part K)
- version 10, from 148 to 152 bytes, for the calibration temperature
- version 9, from 140 to 148 bytes, for the software position limits (`foc-position.md` Software Position Limits)
- version 8, from 132 to 140 bytes, for the position window and its time (`foc-position.md` Target Reached)
- version 7, from 96 to 132 bytes, for the drive-wide limits (`fault-supervision.md` Parts G and H, `foc-speed.md`
  Speed Ramp, `service-can.md` command timeout, `fault-supervision.md` Part I)
- version 6, from 76 to 96 bytes, for the supervision settings (`fault-supervision.md` Part D)
- version 5, from 20 to 76 bytes, for the operator's tuning settings (`controller-selection.md` Part H)

A record written by an earlier firmware reads as invalid once. The start-up settings, the tuning settings, the
default control mode, the algorithm selections, the supervision settings and the limits then return to their
defaults. The next change stores them again. The calibration record is untouched by any of these bumps.

Each region has its own memory region, given to the service when it is built, and can be read, written, and erased independently. An operation on one region has no effect on the others.

### Fault History Region

The fault history (`service-fault-history.md`) is stored differently from the two records above. It is
rewritten in the field, on every fault, where a power loss during a write is to be expected. It has no CRC-32 over
the whole image: a write cut short would invalidate all of it. Each part carries its own check byte instead, so a
cut write loses only the part it was writing.

| Field      | Size    | Description                                                                                            |
|------------|---------|--------------------------------------------------------------------------------------------------------|
| Magic      | 4 bytes | `0xFA17C0DE`                                                                                           |
| Version    | 1 byte  | Layout version of the fault history, 1                                                                 |
| Power-ups  | 2 bytes | Power-up count, saturating at 32767                                                                    |
| Check byte | 1 byte  | CRC-8 of the power-up count                                                                            |
| Records    | 8 × 13  | Code, mode, power-up (2), uptime in ms (4), bus voltage and peak current in hundredths (2 each), CRC-8 |

A record whose check byte does not match, or whose code is none, is an empty slot; an empty slot is written as
all-0xFF. A wrong magic or version reads as a history to be reset: no records, power-up count 0. A write stores the
whole image without erasing it first, then reads it back and compares, as the other regions do. An unchanged part
is rewritten with the same bytes. A power loss can therefore damage only the record or the count that changed.

### Learned Model Region

The learned model of online recalibration (`online-recalibration.md`, Part F) is rewritten in the field, at most
once every 15 minutes of uptime. It is therefore stored as the fault history is. It is one image, written whole
without an erase and read back. It has a check that a write cut short fails.

| Field            | Size    | Description                                                                                      |
|------------------|---------|--------------------------------------------------------------------------------------------------|
| Magic            | 4 bytes | `0x1EA4ED0D`                                                                                     |
| Version          | 1 byte  | Layout version of the learned model, 1                                                           |
| Commissioning    | 4 bytes | CRC-32 of the commissioned inductance, inertia and viscous friction the values were learned from |
| Inductance       | 4 bytes | mH, IEEE-754 single precision                                                                    |
| Inertia          | 4 bytes | kg·m², IEEE-754 single precision                                                                 |
| Viscous friction | 4 bytes | N·m·s/rad, IEEE-754 single precision                                                             |
| Present          | 1 byte  | Bit 0 the inductance, bit 1 the inertia, bit 2 the viscous friction                              |
| CRC              | 4 bytes | CRC-32 of the 22 bytes before it                                                                 |

A wrong magic, version or CRC reads as an empty learned model, one with no quantity present. The image a Reset
Model command stores reads the same way. The commissioning check is compared by online recalibration, not here: the record
does not know the calibration it belongs to.

### On-Media Record Format

Each region stores a single record consisting of a fixed-size header followed by the data payload. The header fields, in order, are:

| Field   | Size    | Description                                      |
|---------|---------|--------------------------------------------------|
| Magic   | 4 bytes | Region identifier (`0xCAFEF00D` or `0xDEADBEEF`) |
| Version | 1 byte  | Layout version constant baked into firmware      |
| CRC-32  | 4 bytes | CRC-32 of the data bytes only (header excluded)  |
| Data    | N bytes | Serialised calibration or configuration record   |

This layout ensures that a calibration record stored in the non-volatile memory cannot be silently misinterpreted as a configuration record, and vice
versa. Any cross-region read fails the magic check immediately.

The header's multi-byte fields are stored least significant byte first. The data is the structure's own bytes,
so the CRC covers exactly the stored bytes. The calibration and configuration records list their four-byte fields
before the smaller ones and fill the last word exactly. Neither therefore has implicit padding whose value would be
undefined. A build-time check on each size holds that. A layout version is bumped whenever a field is added, resized
or reordered. A record written by an earlier firmware then reads as invalid instead of being misinterpreted. The
software-in-the-loop harness builds its NVM images with the same encoding the firmware uses, so the two cannot
drift apart.

All four regions are written the same way: the whole image is written without an erase, read back and compared.
The calibration and configuration records differ from the fault history and the learned model only in what the
image holds.

### Integrity Checks on Read

When a region is read back, the following checks are applied in order:

1. **Magic check** — the four-byte magic field must match the expected value for that region.
2. **Version check** — the version byte must match the version constant compiled into the current firmware.
3. **CRC-32 check** — a CRC-32 is computed over the data bytes and compared to the stored CRC field.

Only if all three checks pass is the data considered valid and *ok* reported; any failure is
*invalid data*. No caller treats a record of another version differently from a damaged one. Either
way, the calibration has to be redone, and a configuration falls back to its defaults. A configuration load
therefore reports *ok* with the defaults when its record is invalid.

These three checks establish that the record is *intact*, not that its values are *usable*. A record whose CRC
matches can still hold a non-finite float. The state machine therefore applies a
plausibility gate on top. The gate requires the electrical fields to be finite (and the phase resistance positive)
before the restored calibration can carry the drive to Ready. The same gate runs on externally supplied
calibration when the external calibration completes, so a value that arrives over CAN cannot be persisted either.
Positive infinity passes every `> 0` test, which is why finiteness is checked explicitly rather than inferred
from a range comparison.

### Write Sequence

Every write stores the whole image without an erase and reads it back. An asynchronous completion from the memory
region drives each step, and no step blocks:

```mermaid
sequenceDiagram
    participant Caller
    participant NVM as Non-volatile memory service
    participant Region as Memory region

    Caller->>NVM: Save calibration
    NVM->>Region: Write image\n(magic + version + CRC + data)
    Region-->>NVM: written
    NVM->>Region: Read back image
    Region-->>NVM: read
    alt read-back equals what was written
        NVM-->>Caller: complete: ok
    else read-back differs
        NVM-->>Caller: complete: hardware fault
    end
```

The read-back comparison catches memory hardware faults, programming errors and marginal cells. Only
the calibration invalidation erases: it erases the calibration region, reads it back, and reports *write failed* if
an intact record survived the erase.

### Address-Space Map

The regions occupy fixed, non-overlapping address ranges within the non-volatile memory. Addresses are byte offsets from the start of its address space.

| Region        | Base address | Size      | Content                  |
|---------------|--------------|-----------|--------------------------|
| Calibration   | 0            | 128 B     | One calibration record   |
| Configuration | 128          | 224 B     | One configuration record |
| Learned model | 352          | 32 B      | The learned model image  |
| Fault history | 384          | 128 B     | The fault history image  |
| Reserved      | 512+         | remainder | Unused; not accessed     |

The regions fit within the 2048-byte internal non-volatile memory of one target. They fill the 512-byte external
non-volatile memory of the reference board, whose target has none of its own. The fault history therefore holds eight
records, the number its 128 bytes take. The configuration region grew from 128 to 256 bytes when the record of
layout 7 outgrew it. It now spans both 256-byte blocks of the external memory. The driver splits a write at the
block boundary. The learned model took the last 32 of its 256 bytes. The 193-byte record of layout 15 fits in the
224 that remain, and no stored record moves. Targets with no non-volatile memory and the host use an in-memory stand-in of
identical layout for regression testing.

### Power-Loss Mitigation

The memory cells retain data indefinitely once a write completes. The write sequence (write record → read-back verify) is designed so that power loss at any point leaves the system in a known state:

- **Power loss during an invalidation's erase**: The region holds erased bytes or a part of the old record. On next read, the magic or the CRC-32 check fails and *invalid data* is returned — the application must re-run identification or use factory defaults.
- **Power loss during write**: The partially-written record will fail either the magic check or the CRC-32 check on next read. The same *invalid data* result is returned; no silent corruption is possible.
- **Power loss during read-back**: The write has already completed. On next power-on, the record will re-read correctly. The application may retry the write if the previous session reported *write failed*.

No journaling or wear-levelling is implemented. Each write operation overwrites the same address range, which is sufficient for infrequent commissioning writes. If frequent writes are required, a wear-levelling strategy must be added at the memory-region layer.

### Buffer Management — No Heap

Each region is held in an EmIL non-volatile image, which owns two statically allocated byte arrays:

- **Write buffer** — sized to hold one complete image; populated before the write to the memory region.
- **Read-back buffer** — same size; populated by the read from the memory region, compared after a write and decoded after a load. The image reports whether the comparison verified; the service maps a failed comparison to *hardware fault*.

Both buffers are fixed-size byte arrays inside the service. They are reused across invocations. No heap is used at any point.

### Concurrency — One Operation At A Time, Device-Wide

All logical regions are backed by a single non-volatile memory device, and that device driver serialises access.
It accepts exactly one erase, program, or read at a time. It treats a second concurrent request as a
programming error. Admission control is therefore **device-wide, not per region**. While any
operation is in flight, every entry point is refused. The entry points are calibration save, load and invalidate,
configuration save and load, and the fault-history and learned-model loads and saves. No refused request ever reaches
a region.

A refused request never silently disappears. Its completion is invoked immediately with *busy*. Callers
that arm a pending-command completion before issuing a request therefore always see it complete, and
cannot be wedged by a dropped notification.

Refusal is decided before any internal state is touched. A caller that issues a further request
from inside a synchronously delivered *busy* completion can therefore only be refused in turn. Re-entry cannot
corrupt an in-flight operation. Admission is released just before the completion of the
in-flight operation runs, so a follow-up request issued from that completion is accepted normally.

Callers are expected to treat *busy* as meaning that nothing was written. Any in-memory state speculatively
updated in anticipation of a successful write must be rolled back. The originating command must be
rejected cleanly rather than escalated to a fault.

### State Machine (Device-Wide)

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Writing : any save
    Idle --> Erasing : invalidate calibration
    Idle --> Reading : any load
    Erasing --> ReadingBack : erase complete
    Writing --> ReadingBack : write complete
    ReadingBack --> Idle : read-back checked (ok, hardware fault or write failed)
    Reading --> Idle : read complete (ok or invalid data)
    Erasing --> Erasing : any further request refused with busy
    Writing --> Writing : any further request refused with busy
    ReadingBack --> ReadingBack : any further request refused with busy
    Reading --> Reading : any further request refused with busy
```

### Error Codes

| Status         | Meaning                                                       |
|----------------|---------------------------------------------------------------|
| Ok             | Operation completed successfully                              |
| Invalid data   | Magic, version or CRC-32 mismatch on read                     |
| Write failed   | An intact calibration record survived an invalidation's erase |
| Hardware fault | The read-back after a write differs from what was written     |
| Busy           | Another operation is in flight; nothing was read or written   |

### Calibration and Configuration Data Fields

**Calibration record** (100 bytes total, layout version 6):

| Field            | Physical unit | Description                                                      |
|------------------|---------------|------------------------------------------------------------------|
| Phase resistance | Ω             | Measured phase resistance                                        |
| Inductance       | mH            | Measured phase inductance, used for both axes                    |
| Flux linkage     | Wb            | Rotor flux linkage, identified from the back-EMF                 |
| Current offsets  | A             | Zero-current reading of each phase current sensor                |
| Inertia          | kg·m²         | Estimated rotor inertia                                          |
| Coulomb friction | N·m           | Dry friction from the cogging step; 0 = no feed-forward          |
| Viscous friction | N·m·s/rad     | Viscous friction coefficient                                     |
| Speed bandwidth  | rad/s         | Not measured: mechanical identification writes a fixed 100 rad/s |
| Cogging scale    | N·m per count | Scale of the cogging map entries                                 |
| Index angle      | rad           | Encoder reading at the index in the aligned frame; NaN when none |
| Pole pairs       | —             | Number of motor pole pairs                                       |
| Stage            | —             | Whether the calibration completed                                |
| Current noise    | mA            | RMS current-sensor noise, two bytes; 0 when not measured         |
| Cogging map      | counts        | Cogging torque over one electrical period, 48 bins               |

The four-byte fields come first, then the current noise, the pole pairs and the stage, which fill one word,
then the cogging map.

**Configuration record** (192 bytes total):

| Field                | Physical unit                   | Description                                                                                                    |
|----------------------|---------------------------------|----------------------------------------------------------------------------------------------------------------|
| CAN node ID          | —                               | Base CAN node ID; the hardware node ID is added at boot                                                        |
| CAN baud rate        | bit/s                           | CAN bus baud rate                                                                                              |
| Telemetry rate       | Hz                              | Rate of the current stream (`service-can.md` Part J): 0 stops it, 1–1000 Hz; default 100                       |
| Encoder resolution   | —                               | Encoder counts per mechanical revolution                                                                       |
| Current loop         | rad/s, A, —                     | Bandwidth (0 = automatic), switching gain, boundary layer                                                      |
| Speed loop           | rad/s, —, s                     | Bandwidth (0 = automatic), speed error and integral weights, observer bandwidth ratio, reference time constant |
| Position loop        | rad/s, —, s                     | Bandwidth (0 = automatic), position error, speed error and integral weights, reference time constant           |
| Supervision          | s, —, rad/s, rad, rad           | Still window, current fraction, demanded speed, position error and still excursion of the fault supervisor     |
| Limits               | rad/s, rad, s, rad/s², A, °C, V | Drive limits; ranges and defaults below the table                                                              |
| Default control mode | —                               | Control mode selected on power-up                                                                              |
| Current algorithm    | —                               | Current-loop algorithm selected on power-up                                                                    |
| Speed algorithm      | —                               | Speed-loop algorithm selected on power-up                                                                      |
| Position algorithm   | —                               | Position-loop algorithm selected on power-up                                                                   |
| Two-step deadbeat    | —                               | Two-step variant of the deadbeat current law                                                                   |
| Stall detection      | —                               | Whether the fault supervisor raises a stall (1, the default) or not (0)                                        |
| Recalibration mode   | —                               | Online recalibration: 0 off (the default), 1 on command, 2 automatic (`online-recalibration.md`)               |
| Homing required      | —                               | Whether position mode starts only once the drive is homed: 0 no (the default), 1 yes (`foc-position.md`)       |
| Circular position    | —                               | Position mode: 0 linear, multi-turn (the default), 1 circular, within one turn (`foc-position.md`)             |

The limits, with their ranges and defaults, are:

- the overspeed limit (`fault-supervision.md` Part G), 1–5000 rad/s, default 1000
- the following-error window, 0–3.2 rad, 0 (off) by default, and time, 0.001–10 s, default 0.1 (Part H)
- the speed acceleration and deceleration, 0–100 000 rad/s², 0 (no limit) by default (`foc-speed.md`)
- the profile velocity, 0–5000 rad/s, the profile acceleration and deceleration, 0–100 000 rad/s², and the profile
  jerk, 0–10 000 000 rad/s³. A jerk of 0 selects the trapezoidal plan. All are 0 by default, which turns the profile off
  (`foc-position.md`, Motion Profile)
- the command timeout, 0–60 s, 0 (off) by default (`service-can.md` Part F)
- the continuous and peak current, 0–100 A, 0 by default (off, and the board's current limit). The peak time is
  0.1–60 s, default 1 (`fault-supervision.md` Part I)
- the position window, 0–3.2 rad, default 6.28 mrad (four counts of the default 4000-count encoder), with its time,
  0–10 s, default 0.02 (`foc-position.md` Target Reached)
- the position minimum and maximum, ±2000π rad (1000 turns), both 0 (off) by default (`foc-position.md` Software
  Position Limits)
- the calibration temperature, −40–100 °C, default 25
- the temperature limit, 40–200 °C, default 120, with its margin, 1–50 K, default 15 (`fault-supervision.md` Part K)
- the bus-voltage minimum and maximum, 0–100 V, both 0 (off) by default (`fault-supervision.md` Part L)
- the quick-stop deceleration, 0–100 000 rad/s², 0 (the current limit) by default (`state-machine.md`, Quick Stop)

The tuning fields hold the operator's values as entered; a bandwidth of 0 asks the firmware to derive it.
They are range-checked when they are written and again at boot. At boot, a value outside its range is replaced by
its default. Such a value is possible only if a later firmware narrows a range without bumping the layout. The four-byte fields precede the byte-sized ones and a
reserved byte completes the last word, so the record has no implicit padding. The supervision fields and the
limits are checked the same way, one field at a time, against the command limits the wire contract also uses.

The four selection fields are stored as raw byte identifiers. A record can pass the integrity check
and still hold a byte outside the valid range for its selection. Each value is therefore range-checked
against its algorithm set before it is applied on power-up. A value outside the range, or a selection
the current motor parameters do not support, leaves the loop on its built-in default. The stored
byte is then corrected to match what is actually running.

---

## Interfaces

### Provided

| Interface              | Purpose                                                    | Contract                                                                |
|------------------------|------------------------------------------------------------|-------------------------------------------------------------------------|
| Save calibration       | Writes the calibration record and verifies the read-back   | Completes exactly once; *busy* if the device is occupied                |
| Load calibration       | Reads and integrity-checks the calibration record          | Completes exactly once; *invalid data* with an empty record when broken |
| Invalidate calibration | Erases the calibration region without writing a new record | A later load reports *invalid data*; completes exactly once             |
| Save configuration     | Same write+verify sequence for the configuration region    | Completes exactly once; *busy* if the device is occupied                |
| Load configuration     | Reads and integrity-checks the configuration record        | Completes exactly once; *ok* with the defaults when broken              |
| Load fault history     | Reads the fault history image and checks each part         | Completes once; *invalid data* for a wrong magic or version, empty      |
| Save fault history     | Writes the fault history image and verifies the read-back  | Completes exactly once; *busy* if the device is occupied                |
| Load learned model     | Reads the learned-model image and checks it                | Completes once; *invalid data* for a wrong magic, version or CRC, empty |
| Save learned model     | Writes the learned-model image and verifies the read-back  | Completes exactly once; *busy* if the device is occupied                |

Each completion carries one of the statuses of Error Codes. In the contracts, "broken" means the stored record is
not intact, and "empty" means the load hands back an empty history or model.

### Required

| Interface                   | Purpose                                               | Contract                                                       |
|-----------------------------|-------------------------------------------------------|----------------------------------------------------------------|
| Calibration memory region   | Raw write and read of the calibration address range   | A dedicated address range, not shared with any other user      |
| Configuration memory region | Raw write and read of the configuration address range | A dedicated address range, separate from the calibration range |
| Fault history memory region | Raw write and read of the fault history address range | A dedicated address range of at least the image's 112 bytes    |
| Learned model memory region | Raw write and read of the learned model address range | A dedicated address range of at least the image's 26 bytes     |
