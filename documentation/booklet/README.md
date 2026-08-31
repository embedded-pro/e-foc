# e-foc — Design Booklet

The design reference for **e-foc**, a real-time Field-Oriented Control firmware for BLDC/PMSM
motors on bare-metal embedded systems. The booklet covers three layers: the mathematical theory that
underpins every control loop, the system architecture that structures the firmware, and the detailed
design of each component.

No source code appears here. Each chapter describes behaviour, structure, and responsibilities at the
level a hardware or control engineer needs to understand the system without reading implementation
files.

## Part I — Theory

| #  | Chapter                                                                               | What it covers                                                     |
|----|---------------------------------------------------------------------------------------|--------------------------------------------------------------------|
| 1  | [Field-Oriented Control](../theory/foc.md)                                            | Clarke/Park transforms, SVM, torque equation, complete FOC loop    |
| 2  | [PMSM Plant Models](../theory/foc-plant-models.md)                                    | Current, speed, and position plant derivations; ZOH discretisation |
| 3  | [Controller Algorithm Index](../theory/advanced-controllers.md)                       | Algorithm map, parameter sources, per-algorithm navigation         |
| 4  | [Current Loop — PI (baseline)](../theory/current-loop-pi.md)                          | Pole-zero cancellation, gain normalisation, anti-windup            |
| 5  | [Current Loop — Decoupled PID (A1)](../theory/current-loop-decoupled-pid.md)          | Cross-coupling + back-EMF feedforward                              |
| 6  | [Current Loop — Deadbeat (A2)](../theory/current-loop-deadbeat.md)                    | 1-step and 2-step plant inversion; maximum servo stiffness         |
| 7  | [Current Loop — Sliding-Mode (A3)](../theory/current-loop-sliding-mode.md)            | Robust to Rs/Ls mismatch; boundary-layer saturation                |
| 8  | [Speed Loop — PI (baseline)](../theory/speed-loop-pi.md)                              | Speed plant, pole-zero cancellation, anti-windup                   |
| 9  | [Speed Loop — LQI (S1)](../theory/speed-loop-lqi.md)                                  | DARE-optimal gains from J, Bf; systematic tuning                   |
| 10 | [Speed Loop — ADRC (S2)](../theory/speed-loop-adrc.md)                                | Extended State Observer; explicit disturbance cancellation         |
| 11 | [Speed Loop — Two-DOF (S3)](../theory/speed-loop-two-dof.md)                          | Decoupled tracking and stiffness via reference pre-filter          |
| 12 | [Position Loop — P (baseline)](../theory/position-loop-pid.md)                        | Pure-P controller; bandwidth = Kp; angle-error wrapping            |
| 13 | [Position Loop — LQR/LQI (P1)](../theory/position-loop-lqr-lqi.md)                    | DARE-optimal simultaneous θ, ω regulation                          |
| 14 | [Position Loop — Cascade P (P2)](../theory/position-loop-cascade-p.md)                | Industry-standard Kv architecture; velocity feedforward            |
| 15 | [Position Loop — Two-DOF (P3)](../theory/position-loop-two-dof.md)                    | Decoupled tracking and stiffness; angle-wrapping pre-filter        |
| 16 | [Position Loop — ILC (P4)](../theory/position-loop-ilc.md)                            | Near-zero error on repetitive tasks after learning cycles          |
| 17 | [Position Loop — Friction Compensation](../theory/position-loop-friction.md)          | Coulomb + Stribeck feedforward; eliminates hunting at rest         |
| 18 | [Resistance and Inductance Estimation](../theory/resistance-inductance-estimation.md) | RLS-based online electrical parameter identification               |
| 19 | [Friction and Inertia Estimation](../theory/friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification               |
| 20 | [Motor Alignment](../theory/alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration        |

## Part II — Architecture

| #  | Chapter                                          | What it covers                                                  |
|----|--------------------------------------------------|-----------------------------------------------------------------|
| 10 | [System Architecture](../architecture/system.md) | Layer decomposition, component ownership, real-time constraints |

## Part III — Design

| #  | Chapter                                                                                | What it covers                                                  |
|----|----------------------------------------------------------------------------------------|-----------------------------------------------------------------|
| 11 | [FOC Mathematical Transforms](../design/foc-transforms.md)                             | Clarke, Park, inverse Park, SVM — component responsibilities    |
| 12 | [FOC Torque Control](../design/foc-torque.md)                                          | Current-loop component design, PI anti-windup, ISR flow         |
| 13 | [FOC Speed Control](../design/foc-speed.md)                                            | Speed-loop component design, 1 kHz handler flow                 |
| 14 | [FOC Position Control](../design/foc-position.md)                                      | Position-loop component design, handler flow                    |
| 15 | [Runtime Controller Selection](../design/controller-selection.md)                      | Heap-free variant storage, std::visit dispatch, state gating    |
| 16 | [Service: FOC State Machine](../design/state-machine.md)                               | States, transitions, guards, and the enable/disable lifecycle   |
| 17 | [Service: Motor Alignment](../design/service-alignment.md)                             | Alignment sequence, completion signalling, error handling       |
| 18 | [Service: Electrical Parameters Identification](../design/service-electrical-ident.md) | RLS service design, result storage, calibration lifecycle       |
| 19 | [Service: Mechanical Parameters Identification](../design/service-mechanical-ident.md) | Mechanical RLS service design and excitation strategy           |
| 20 | [Service: Non-Volatile Memory](../design/service-nvm.md)                               | NVM layout, versioning, parameter persistence                   |
| 21 | [Service: Command-Line Interface](../design/service-cli.md)                            | CLI command routing, parameter read/write, observer interface   |
| 22 | [CAN Service Layer](../design/service-can.md)                                          | CAN service design, message dispatch, FOC command/response flow |
| 23 | [Error Handling](../design/error-handling.md)                                          | Error taxonomy, propagation, recovery, and safe-state entry     |
| 24 | [Integration Testing Design](../design/integration-testing.md)                         | Test architecture, fixture composition, coverage strategy       |

## Building the booklet

Install the dependencies used by CI (mermaid-cli, Pandoc, and a XeLaTeX toolchain), then build
both outputs:

```
python scripts/build-foc-booklet.py --format all
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
2. Create the file with a single top-level heading and section numbering (`## 1.`, `## 2.`).
3. Link it from the table above under the correct Part — the index drives both outputs.
4. Author diagrams as ```` ```mermaid ```` fences. A fence that fails to parse fails the build.
