# FOC Theory — Chapter Index

This index controls the chapter order in the published booklet.
The build script reads links from this file in order to assemble the book.

| Chapter | File                                                                        | Content                                                            |
|---------|-----------------------------------------------------------------------------|--------------------------------------------------------------------|
| 1       | [Field-Oriented Control](foc.md)                                            | Clarke/Park transforms, SVM, PI current control, torque equation   |
| 2       | [PMSM Plant Models](foc-plant-models.md)                                    | Current, speed, and position plant derivations; ZOH discretization |
| 3       | [Advanced Controllers — Overview](advanced-controllers.md)                  | Algorithm map, parameter sources, summary tables                   |
| 4       | [Current Loop Controllers](current-loop-controllers.md)                     | Decoupled PID, Deadbeat, Sliding-mode                              |
| 5       | [Speed Loop Controllers](speed-loop-controllers.md)                         | LQI, ADRC, Two-DOF                                                 |
| 6       | [Position Loop Controllers](position-loop-controllers.md)                   | LQR/LQI, Cascade P, Two-DOF, ILC, Friction compensation            |
| 7       | [Resistance and Inductance Estimation](resistance-inductance-estimation.md) | RLS-based online electrical parameter identification               |
| 8       | [Friction and Inertia Estimation](friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification               |
| 9       | [Motor Alignment](alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration        |
