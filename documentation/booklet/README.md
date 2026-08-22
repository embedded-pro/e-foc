# e-foc — Design Booklet

The design reference for **e-foc**, a real-time Field-Oriented Control firmware for BLDC/PMSM
motors on bare-metal embedded systems. The booklet covers three layers: the mathematical theory that
underpins every control loop, the system architecture that structures the firmware, and the detailed
design of each component.

No source code appears here. Each chapter describes behaviour, structure, and responsibilities at the
level a hardware or control engineer needs to understand the system without reading implementation
files.

## Part I — Theory

| # | Chapter                                                                               | What it covers                                                              |
|---|---------------------------------------------------------------------------------------|-----------------------------------------------------------------------------|
| 1 | [Field-Oriented Control](../theory/foc.md)                                            | Clarke/Park transforms, SVM, PI current control, complete FOC loop          |
| 2 | [PMSM Plant Models](../theory/foc-plant-models.md)                                    | Current, speed, and position plant derivations; ZOH discretisation          |
| 3 | [Advanced Controllers — Overview](../theory/advanced-controllers.md)                  | Algorithm map, parameter sources, summary tables for all three loops        |
| 4 | [Current Loop Controllers](../theory/current-loop-controllers.md)                     | Decoupled PID (A1), Deadbeat (A2), Sliding-mode (A3)                        |
| 5 | [Speed Loop Controllers](../theory/speed-loop-controllers.md)                         | LQI (S1), ADRC (S2), Two-DOF (S3)                                           |
| 6 | [Position Loop Controllers](../theory/position-loop-controllers.md)                   | LQR/LQI (P1), Cascade P (P2), Two-DOF (P3), ILC (P4), Friction compensation |
| 7 | [Resistance and Inductance Estimation](../theory/resistance-inductance-estimation.md) | RLS-based online electrical parameter identification                        |
| 8 | [Friction and Inertia Estimation](../theory/friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification                        |
| 9 | [Motor Alignment](../theory/alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration                 |

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
