# FOC Theory — Chapter Index

The theory chapters in reading order. The booklet takes its order from `documentation/booklet/README.md`,
which lists the same chapters; keep the two in step.

## Foundations

| Chapter                                                             | What it covers                                                                     |
|---------------------------------------------------------------------|------------------------------------------------------------------------------------|
| [The Permanent-Magnet Motor — Physical Picture](motor-physics.md)   | Coils, magnets, the rotating field, torque angle, back-EMF, six-step vs sinusoidal |
| [PMSM Mathematical Model](motor-model.md)                           | abc → space vector → αβ → dq; torque; mechanics; operating envelope                |
| [Closed-Loop Control of a PMSM](closed-loop-control.md)             | S and T, the cascade, feedback signals, anti-windup, sampling and delay            |
| [Field-Oriented Control](foc.md)                                    | Transforms as computed, SVM, lookup table, complete FOC loop                       |
| [PMSM Plant Models](foc-plant-models.md)                            | Current, speed and position plants; ZOH discretisation; reference motors           |
| [FOC Controllers — Map and Trade-offs](advanced-controllers.md)     | What each algorithm assumes, gains and costs; decision flow                        |
| [The Numbers That Matter](numbers-to-know.md)                       | Time scales, constants, voltage budget, resolutions, rules of thumb                |
| [Disturbances, Non-Idealities and Noise](disturbances-and-noise.md) | Where errors enter, their frequency signatures, mitigations                        |

## Practical Implementation

| Chapter                                                                                           | What it covers                                                                                 |
|---------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| [The Power Stage — MOSFETs, Half-Bridges and Dead Time](power-stage.md)                           | Switches, gate drive and bootstrap, shoot-through, dead time, losses, DC link, protection      |
| [The Sensing Chain — Shunts, Amplifiers, ADC and Encoder](sensing-chain.md)                       | Shunt topologies, op-amp offset and drift, gain errors, ADC timing, encoder, error budget      |
| [Low-Side Shunt Current Sensing and How to Configure the PWM for It](low-side-current-sensing.md) | Shunt states, the common 000 window, hidden phases, duty shifting, PWM and ADC checklist       |
| [Cogging and Other Motor Imperfections](cogging-and-motor-imperfections.md)                       | Cogging periodicity, effects, measurement and feed-forward; harmonics, saturation, temperature |
| [From Equations to Firmware — Practical Implementation](firmware-implementation.md)               | Interrupt structure, cycle budget, duty delay, numerics, determinism, calibration order        |

## Controller and Identification Reference

| Chapter                                                                     | What it covers                                              |
|-----------------------------------------------------------------------------|-------------------------------------------------------------|
| [Current Loop — PI (baseline)](current-loop-pi.md)                          | Pole-zero cancellation, gain normalisation, anti-windup     |
| [Current Loop — Decoupled PID (A1)](current-loop-decoupled-pid.md)          | Cross-coupling + back-EMF feedforward                       |
| [Current Loop — Deadbeat (A2)](current-loop-deadbeat.md)                    | 1-step and 2-step plant inversion; maximum servo stiffness  |
| [Current Loop — Sliding-Mode (A3)](current-loop-sliding-mode.md)            | Robust to Rs/Ls mismatch; boundary-layer saturation         |
| [Speed Loop — PI (baseline)](speed-loop-pi.md)                              | Speed plant, pole-zero cancellation, anti-windup            |
| [Speed Loop — LQI (S1)](speed-loop-lqi.md)                                  | DARE-optimal gains from J, Bf; systematic tuning            |
| [Speed Loop — ADRC (S2)](speed-loop-adrc.md)                                | Extended State Observer; explicit disturbance cancellation  |
| [Speed Loop — Two-DOF (S3)](speed-loop-two-dof.md)                          | Decoupled tracking and stiffness via reference pre-filter   |
| [Position Loop — P (baseline)](position-loop-pid.md)                        | Pure-P controller; bandwidth = Kp; angle-error wrapping     |
| [Position Loop — LQR/LQI (P1)](position-loop-lqr-lqi.md)                    | DARE-optimal simultaneous θ, ω regulation                   |
| [Position Loop — Cascade P (P2)](position-loop-cascade-p.md)                | Industry-standard Kv architecture; velocity feedforward     |
| [Position Loop — Two-DOF (P3)](position-loop-two-dof.md)                    | Decoupled tracking and stiffness; angle-wrapping pre-filter |
| [Resistance and Inductance Estimation](resistance-inductance-estimation.md) | RLS-based online electrical parameter identification        |
| [Friction and Inertia Estimation](friction-inertia-estimation.md)           | RLS-based online mechanical parameter identification        |
| [Motor Alignment](alignment.md)                                             | Rotor flux alignment procedure and angle offset calibration |
