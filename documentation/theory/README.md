# FOC Theory — Chapter Index

This index controls the chapter order in the published booklet.
The build script reads links from this file in order to assemble the book.

| Chapter | File                                                                        | Content                                                            |
|---------|-----------------------------------------------------------------------------|--------------------------------------------------------------------|
| 1       | [Field-Oriented Control](foc.md)                                            | Clarke/Park transforms, SVM, torque equation, complete FOC loop    |
| 2       | [PMSM Plant Models](foc-plant-models.md)                                    | Current, speed, and position plant derivations; ZOH discretization |
| 3       | [Controller Algorithm Index](advanced-controllers.md)                       | Algorithm map, parameter sources, per-algorithm navigation         |
| 4       | [Current Loop — PI (baseline)](current-loop-pi.md)                          | Pole-zero cancellation, gain normalisation, anti-windup            |
| 5       | [Current Loop — Decoupled PID (A1)](current-loop-decoupled-pid.md)          | Cross-coupling + back-EMF feedforward                              |
| 6       | [Current Loop — Deadbeat (A2)](current-loop-deadbeat.md)                    | 1-step and 2-step plant inversion; maximum servo stiffness         |
| 7       | [Current Loop — Sliding-Mode (A3)](current-loop-sliding-mode.md)            | Robust to Rs/Ls mismatch; boundary-layer saturation                |
| 8       | [Speed Loop — PI (baseline)](speed-loop-pi.md)                              | Speed plant, pole-zero cancellation, anti-windup                   |
| 9       | [Speed Loop — LQI (S1)](speed-loop-lqi.md)                                  | DARE-optimal gains from J, Bf; systematic tuning                   |
| 10      | [Speed Loop — ADRC (S2)](speed-loop-adrc.md)                                | Extended State Observer; explicit disturbance cancellation         |
| 11      | [Speed Loop — Two-DOF (S3)](speed-loop-two-dof.md)                          | Decoupled tracking and stiffness via reference pre-filter          |
| 12      | [Position Loop — P (baseline)](position-loop-pid.md)                        | Pure-P controller; bandwidth = Kp; angle-error wrapping            |
| 13      | [Position Loop — LQR/LQI (P1)](position-loop-lqr-lqi.md)                    | DARE-optimal simultaneous θ, ω regulation                          |
| 14      | [Position Loop — Cascade P (P2)](position-loop-cascade-p.md)                | Industry-standard Kv architecture; velocity feedforward            |
| 15      | [Position Loop — Two-DOF (P3)](position-loop-two-dof.md)                    | Decoupled tracking and stiffness; angle-wrapping pre-filter        |
| 16      | [Position Loop — ILC (P4)](position-loop-ilc.md)                            | Near-zero error on repetitive tasks after learning cycles          |
| 17      | [Position Loop — Friction Compensation](position-loop-friction.md)          | Coulomb + Stribeck feedforward; eliminates hunting at rest         |
| 18      | [Resistance and Inductance Estimation](resistance-inductance-estimation.md) | RLS-based online electrical parameter identification               |
| 19      | [Friction and Inertia Estimation](friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification               |
| 20      | [Motor Alignment](alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration        |
