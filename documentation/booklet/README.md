# e-foc — Design Booklet

The design reference for **e-foc**, a real-time Field-Oriented Control firmware for BLDC/PMSM motors on
bare-metal embedded systems. The booklet has five parts. The theory foundations read in order, from the
motor itself to the closed loops around it, the trade-offs between controllers, the numbers that matter
and the disturbances the loops must live with. The practical part covers what the hardware and the
firmware add: the power stage, the sensing chain and low-side shunts, the motor's imperfections and the
implementation. The theory reference holds one chapter per controller algorithm and identification
method. The architecture part structures the firmware, and the design part describes each component.

No source code appears here. Each chapter describes behaviour, structure, and responsibilities at the
level a hardware or control engineer needs to understand the system without reading implementation
files. The builder numbers the chapters in the order the tables below list them.

## Theory — Foundations

| Chapter                                                                       | What it covers                                                                     |
|-------------------------------------------------------------------------------|------------------------------------------------------------------------------------|
| [The Permanent-Magnet Motor — Physical Picture](../theory/motor-physics.md)   | Coils, magnets, the rotating field, torque angle, back-EMF, six-step vs sinusoidal |
| [PMSM Mathematical Model](../theory/motor-model.md)                           | abc → space vector → αβ → dq; torque; mechanics; operating envelope                |
| [Closed-Loop Control of a PMSM](../theory/closed-loop-control.md)             | S and T, the cascade, feedback signals, anti-windup, sampling and delay            |
| [Field-Oriented Control](../theory/foc.md)                                    | Transforms as computed, SVM, lookup table, complete FOC loop                       |
| [PMSM Plant Models](../theory/foc-plant-models.md)                            | Current, speed and position plants; ZOH discretisation; reference motors           |
| [FOC Controllers — Map and Trade-offs](../theory/advanced-controllers.md)     | What each algorithm assumes, gains and costs; decision flow                        |
| [The Numbers That Matter](../theory/numbers-to-know.md)                       | Time scales, constants, voltage budget, resolutions, rules of thumb                |
| [Disturbances, Non-Idealities and Noise](../theory/disturbances-and-noise.md) | Where errors enter, their frequency signatures, mitigations                        |

## Theory — Practical Implementation

| Chapter                                                                                                     | What it covers                                                                                 |
|-------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| [The Power Stage — MOSFETs, Half-Bridges and Dead Time](../theory/power-stage.md)                           | Switches, gate drive and bootstrap, shoot-through, dead time, losses, DC link, protection      |
| [The Sensing Chain — Shunts, Amplifiers, ADC and Encoder](../theory/sensing-chain.md)                       | Shunt topologies, op-amp offset and drift, gain errors, ADC timing, encoder, error budget      |
| [Low-Side Shunt Current Sensing and How to Configure the PWM for It](../theory/low-side-current-sensing.md) | Shunt states, the common 000 window, hidden phases, duty shifting, PWM and ADC checklist       |
| [Cogging and Other Motor Imperfections](../theory/cogging-and-motor-imperfections.md)                       | Cogging periodicity, effects, measurement and feed-forward; harmonics, saturation, temperature |
| [From Equations to Firmware — Practical Implementation](../theory/firmware-implementation.md)               | Interrupt structure, cycle budget, duty delay, numerics, determinism, calibration order        |

## Theory — Controller and Identification Reference

| Chapter                                                                               | What it covers                                              |
|---------------------------------------------------------------------------------------|-------------------------------------------------------------|
| [Current Loop — PI (baseline)](../theory/current-loop-pi.md)                          | Pole-zero cancellation, gain normalisation, anti-windup     |
| [Current Loop — Decoupled PID (A1)](../theory/current-loop-decoupled-pid.md)          | Cross-coupling + back-EMF feedforward                       |
| [Current Loop — Deadbeat (A2)](../theory/current-loop-deadbeat.md)                    | 1-step and 2-step plant inversion; maximum servo stiffness  |
| [Current Loop — Sliding-Mode (A3)](../theory/current-loop-sliding-mode.md)            | Robust to Rs/Ls mismatch; boundary-layer saturation         |
| [Speed Loop — PI (baseline)](../theory/speed-loop-pi.md)                              | Speed plant, pole-zero cancellation, anti-windup            |
| [Speed Loop — LQI (S1)](../theory/speed-loop-lqi.md)                                  | DARE-optimal gains from J, Bf; systematic tuning            |
| [Speed Loop — ADRC (S2)](../theory/speed-loop-adrc.md)                                | Extended State Observer; explicit disturbance cancellation  |
| [Speed Loop — Two-DOF (S3)](../theory/speed-loop-two-dof.md)                          | Decoupled tracking and stiffness via reference pre-filter   |
| [Position Loop — P (baseline)](../theory/position-loop-pid.md)                        | Pure-P controller; bandwidth = Kp; angle-error wrapping     |
| [Position Loop — LQR/LQI (P1)](../theory/position-loop-lqr-lqi.md)                    | DARE-optimal simultaneous θ, ω regulation                   |
| [Position Loop — Cascade P (P2)](../theory/position-loop-cascade-p.md)                | Industry-standard Kv architecture; velocity feedforward     |
| [Position Loop — Two-DOF (P3)](../theory/position-loop-two-dof.md)                    | Decoupled tracking and stiffness; angle-wrapping pre-filter |
| [Position Loop — ILC (P4)](../theory/position-loop-ilc.md)                            | Near-zero error on repetitive tasks after learning cycles   |
| [Position Loop — Friction Compensation](../theory/position-loop-friction.md)          | Coulomb + Stribeck feedforward; eliminates hunting at rest  |
| [Resistance and Inductance Estimation](../theory/resistance-inductance-estimation.md) | RLS-based online electrical parameter identification        |
| [Friction and Inertia Estimation](../theory/friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification        |
| [Motor Alignment](../theory/alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration |

## Architecture

| Chapter                                          | What it covers                                                  |
|--------------------------------------------------|-----------------------------------------------------------------|
| [System Architecture](../architecture/system.md) | Layer decomposition, component ownership, real-time constraints |

## Design

| Chapter                                                                                | What it covers                                                                      |
|----------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| [FOC Mathematical Transforms](../design/foc-transforms.md)                             | Clarke, Park, inverse Park, SVM — component responsibilities                        |
| [FOC Torque Control](../design/foc-torque.md)                                          | Current-loop component design, PI anti-windup, ISR flow                             |
| [FOC Speed Control](../design/foc-speed.md)                                            | Speed-loop component design, 1 kHz handler flow                                     |
| [FOC Position Control](../design/foc-position.md)                                      | Position-loop component design, handler flow                                        |
| [Runtime Controller Selection](../design/controller-selection.md)                      | Heap-free variant storage, std::visit dispatch, state gating                        |
| [Service: FOC State Machine](../design/state-machine.md)                               | States, transitions, guards, and the enable/disable lifecycle                       |
| [Service: Motor Alignment](../design/service-alignment.md)                             | Alignment sequence, completion signalling, error handling                           |
| [Service: Electrical Parameters Identification](../design/service-electrical-ident.md) | RLS service design, result storage, calibration lifecycle                           |
| [Service: Rotor Standstill](../design/service-standstill.md)                           | Waits for the rotor to come to rest before the offsets are measured                 |
| [Service: Current Sensor Offsets](../design/service-current-offsets.md)                | Offset measurement at calibration, correction of every phase-current sample         |
| [Service: Dead-Time Compensation](../design/service-dead-time-compensation.md)         | Correction of every duty for the bridge's dead-time loss, the board's figures       |
| [Platform: Phase-Current Sample Rate](../design/platform-phase-current-rate.md)        | Per-period platform samples, decimated in one place to the rate a consumer asks for |
| [Service: Cogging Compensation](../design/service-cogging-compensation.md)             | Cogging torque map at calibration, current feed-forward in the cascades             |
| [Service: Flux Linkage Identification](../design/service-flux-linkage-ident.md)        | Flux linkage from the back-EMF while the rotor coasts, before the mechanical step   |
| [Service: Mechanical Parameters Identification](../design/service-mechanical-ident.md) | Mechanical RLS service design and excitation strategy                               |
| [Service: Non-Volatile Memory](../design/service-nvm.md)                               | NVM layout, versioning, parameter persistence                                       |
| [Service: Command-Line Interface](../design/service-cli.md)                            | CLI command routing, parameter read/write, observer interface                       |
| [CAN Service Layer](../design/service-can.md)                                          | CAN service design, message dispatch, FOC command/response flow                     |
| [Error Handling](../design/error-handling.md)                                          | Error taxonomy, propagation, recovery, and safe-state entry                         |
| [Watchdog](../design/watchdog.md)                                                      | Event dispatcher supervised by the MCU watchdog, power-stage cutoff on expiry       |
| [Integration Testing Design](../design/integration-testing.md)                         | Test architecture, fixture composition, coverage strategy                           |
| [Tool: CAN Commander](../design/tool-can-commander.md)                                 | Desktop commander, issue log, update check, Windows and Debian installers           |

## Building the booklet

Install the dependencies used by CI (mermaid-cli, Pandoc, and a XeLaTeX toolchain), then build
both outputs:

```
python documentation/tools/build-foc-booklet.py --format all
```

Outputs land in `build/booklet/`:

| Path              | Content                                             |
|-------------------|-----------------------------------------------------|
| `eFocDesign.pdf`  | The complete booklet as a single PDF                |
| `site/index.html` | Static site: landing page plus one page per chapter |
| `book.md`         | Assembled Markdown the PDF is rendered from         |

`--format pdf` and `--format html` build one output; `--skip-diagrams` leaves Mermaid fences as
code blocks when mmdc is unavailable; `--assemble-only` stops after writing `book.md`.

CI builds both outputs on every pull request that touches `documentation/`, publishes the site to
GitHub Pages on `main`, and attaches the PDF to every published release.

## Adding or changing a chapter

1. Check that the material does not belong to an existing document. Theory belongs in
   `documentation/theory/`, architecture decisions in `documentation/architecture/system.md`,
   component design in `documentation/design/`. No source code in any chapter.
2. Create the file from its template in `documentation/templates/`. Keep the template's `##` section
   headings exactly as written (the documentation validator looks for them); number the `###`
   subsections beneath them (`### 1.`, `### 2.`).
3. Link it from the table above under the correct Part — the index drives both outputs.
4. Author diagrams as ```` ```mermaid ```` fences. A fence that fails to parse fails the build.
   Theory chapters may also embed animations from `documentation/animations/`
   (`python documentation/tools/generate-animations.py`); the site shows them animated, the PDF prints
   their `-frames.svg` strip, and a missing strip fails the build.
