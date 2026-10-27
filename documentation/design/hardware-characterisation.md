---
title: "Hardware Characterisation and the Bench Motor"
type: design
status: draft
version: 0.1.0
component: sil-calibration
date: 2026-10-19
---

| Field     | Value                                         |
|-----------|-----------------------------------------------|
| Title     | Hardware Characterisation and the Bench Motor |
| Type      | design                                        |
| Status    | draft                                         |
| Version   | 0.1.0                                         |
| Component | sil-calibration                               |
| Date      | 2026-10-19                                    |

---

## Responsibilities

**Is responsible for:**

- Defining what is measured on the real rig to learn its encoder and current-sensing noise
- Defining how those measurements become the parameters of the SIL `bench` plant
- Defining how the same scenario is compared between the emulated and the real target
- Recording the targets the firmware does not meet yet, with the scenario that will hold each

**Is NOT responsible for:**

- Changing the estimators; each target below is fixed in its own change
- Datasheet values: the rig's motor is generic and its published parameters are not trusted

---

## Component Details

### Part A — What is known about the bench motor

| Quantity            | Value            | Provenance                                                                                                                         |
|---------------------|------------------|------------------------------------------------------------------------------------------------------------------------------------|
| Resistance          | 1.10 Ω           | Identified by the firmware, stable to 0.3 % over 38 runs                                                                           |
| d-axis inductance   | 1.07 mH          | Identified; the same procedure reads 11 % low on the SIL bench plant, so the rig's true value may be near 1.2 mH                   |
| Pole pairs          | 4                | Identified                                                                                                                         |
| Encoder             | 4000 counts      | Observed from the position steps                                                                                                   |
| Inertia (with disk) | 12.4 µkg·m²      | Identified with the weighting bias of Part D, about 10 % low; the true value is nearer 13.8 µ until the rig is re-run with the fix |
| Flux linkage        | 0.007 Wb         | **Not measured**: derived from a claimed 0.042 Nm/A                                                                                |
| Viscous friction    | 5 µNm·s/rad      | **Not measured**: a guess                                                                                                          |
| Coulomb friction    | 0.5 mNm          | **Not measured**: a guess                                                                                                          |
| Supply              | 24 V, 4 A → 10 A | The 4 A supply collapsed during runs; the 10 A one is pending                                                                      |

The three guesses are measured in Part C before the plant is trusted for anything but regression.

### Part B — Noise measurement plan

Every condition records at least 131 072 samples, so a standard deviation is known to 0.5 % and
a spectrum resolves 0.15 Hz at 20 kHz. Each is repeated over three power cycles, once cold and
once after ten minutes of PWM, to separate noise from drift.

**Current sensing** (PWM running, motor connected, no commanded current):

| Condition                                                   | Learns                                                |
|-------------------------------------------------------------|-------------------------------------------------------|
| PWM off                                                     | Offset and noise of the amplifier and converter alone |
| PWM on at 50 % duty, rotor locked                           | Switching noise added to the same                     |
| PWM on, sampled at the centre versus the edge of the period | How much the sampling instant matters                 |
| A known DC current, read by a bench meter                   | Gain error per phase                                  |
| Phase pairs                                                 | Correlation between channels                          |

**Encoder** (rotor free, disk fitted):

| Condition                                           | Learns                                                                                                   |
|-----------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| At rest, unpowered                                  | Count dither: how often the reading flips between neighbours                                             |
| At rest, holding current                            | Noise coupled in from the power stage                                                                    |
| Constant speed at 10, 50 and 100 rad/s              | Residual of the angle against a fitted line: the count-step regularity and the once-per-revolution error |
| The same, differenced over 1, 10, 20 and 80 samples | The acceleration noise the estimator sees at each window                                                 |

The last row is the number the SIL needs most: it fixes how much noise the acceleration carries
at each window. It is compared against the acceleration noise the plant predicts from its own
count size.

**How to capture.** The control interrupt writes into a RAM ring only while a capture is armed,
so the hot path costs one predictable branch otherwise. A terminal command arms it for a given
channel, sample count and decimation, then streams the ring out over RTT (port 2334). A build
flag keeps the command out of production images. The analogue side is recorded in parallel on the
AD3 scope with `tools/hil/ad3_monitor.py`, which separates amplifier noise from the converter's
own. Ring size is bounded by RAM: 16 384 samples of four channels at two bytes is 128 KiB, so a
131 072-sample record is eight captures concatenated at a fixed condition.

**Output.** One JSON per condition under `documentation/design/measurements/`: mean, standard
deviation, a 64-bin histogram, and the power spectrum in 64 log-spaced bins. The plant presets
read the standard deviations and offsets from it. Two plant fields are missing and are added when
the data asks for them: a periodic encoder error (amplitude and order) and a slow offset drift.

### Part C — Measuring what is guessed

| Parameter        | How                                                                                                                                                                                          |
|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Flux linkage     | Spin the rotor open-loop at three speeds with the phases floating, read the back-EMF on the AD3                                                                                              |
| Coulomb friction | The smallest q-axis current that breaks the rotor away, in both directions                                                                                                                   |
| Viscous friction | Torque against speed at steady state over five speeds, minus the Coulomb term                                                                                                                |
| True inertia     | Add a steel disk of known mass and radius, J = ½·m·r², and compare the change the firmware reports. A bias in the estimator shows as a ratio other than one, with no model of the rig needed |

### Part D — Targets

The bench plant found three defects.

**Fixed: the inertia read about 10 % low** on a 4000-count encoder, 22 % at 1000 counts. The
first diagnosis was noise in the acceleration regressor. A simulation of the estimator alone
showed otherwise: with a continuous encoder and no noise, an 80-sample window already gave 0.87
of the true inertia, and quantisation moved it by one point. The cause is the weighting. The
acceleration between two window speeds is weighted by a triangle over both windows; the torque
beside it was a plain mean of the two window torques, which differs wherever the torque bends.
Weighting the torque by the same triangle gave 0.99 in the simulation and 0.03 % to 1.6 % on the
reference motors in SIL, and is what the firmware does now. At 1000 counts 7 % remained. The
speed regressor being a plain mean was a guess; the simulation of the estimator showed the
window was the cause, clamped at 128 samples where the encoder asked for 158. With the limit at
256 samples the bench motor read 1.9 % off at 1000 counts and the reference rotors within 3.2 %.
The scenarios are in `identification_robustness.feature`.

**Fixed: a residual low inertia on quantised encoders that varied from run to run**: 1.4 % to
4.4 % low over eight runs at 1000 counts, about 1 % at 4000 and 20000 counts, and within 0.15 %
on a continuous encoder. CI read 4.4 % against the 4 % limit the 1.9 % run had pinned. The
quantisation the first diagnosis suspected was real after all, but in the speeds: they were
differences of single end samples, so every acceleration carried about 70 rad/s² of
quantisation noise, and noise in a regressor biases a least-squares coefficient toward zero.
Speeds now come from window-mean positions, with the torque weighted by the matching quadratic
kernel over three windows (`service-mechanical-ident.md`). The eight 1000-count runs then read
0.09 % to 0.17 % low, every quantised row of the scenario within 0.3 %, and the scenario pins
them at 1 %. Aligning the torque exactly also exposed a plant whose rotor moved a sample ahead
of its reported current, which had read as friction (`software-in-the-loop.md`, Part K). The
quantisation noise had also been counted as excitation on the speed plateaus, so the count the
run needs is now scaled by the window. In the estimator's unit test a 1000-count run converges
in 20 700 samples, against 36 900 before the change and 111 000 with window means but no
scaling.

**Fixed: a calibration of the light reference rotor under 0.2 mrad of encoder noise ended in
Fault**, with inertia about 100× too small. A trace confirmed the cause: the window is chosen
from the smallest position step the encoder shows, which is one count for a quantised encoder and
shrinks without bound for a noisy continuous one, so it fell to 3 to 7 samples and the
acceleration was mostly noise. The window now comes from the larger of that step and the step
the encoder's noise is equivalent to, measured from the second difference of the position. The
light rotor then identifies within 2.2 %. The scenario is the teknic plant with
`encoder_noise_sigma_radians` of 0.0002 in `identification_robustness.feature`.

### Part E — The bench motor as a first-class SIL motor

1. Move the plant out of `PlantPresets.hpp` into `motor_parameters/` beside the two reference
   motors, with the provenance table of Part A in its header comment.
2. Add it as a third row in the motor tables of the identification, protection and
   control-performance features. The suite takes twenty minutes today, so the rows go in
   behind a count: identification and protection first, the loop sweeps after a measured cost.
3. Run the same feature text against the emulated and the real target. Every identification
   step already prints a `[METRIC]` line; a small script pairs the lines of the two runs and
   reports the gap per quantity. That gap is the figure of merit for the plant: it falls as
   Parts B and C land, and a regression in it says the model has drifted from the rig.

### Part F — Duty limits of the E-FOC-HARDWARE board

The board's `BoardCharacteristics` carries two duty limits: one measured on the rig and one set by the gate driver.

| Limit            | Value | Provenance                                                                                                                                      |
|------------------|-------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| Maximum readable | 70 %  | Above it a phase's low-side window at the PWM centre is too short for its shunt: a phase read within 0.25 A at 71.5 % and 1 A low at 79.7 %     |
| Maximum duty     | 95 %  | The FAN7888 charges each high-side bootstrap capacitor only while the low side conducts; 95 % leaves 2.5 µs of every period, less the dead band |

The current loop lost a phase above the readable limit, which is why `foc::KeepLowSideShuntsReadable` lowers the duties
(`documentation/architecture/system.md`).

---

## Interfaces

### Provided

- The `bench` plant preset (`integration_tests/support/plant/PlantPresets.hpp`), whose values Part A traces to the rig
- The noise figures of Parts B and C, as the sigma and bias inputs of the plant's ADC and encoder noise settings
- The scenarios of `identification_robustness.feature`, which hold each target of Part D once its fix lands

### Required

- A capture path from the board: a RAM ring read over RTT, with the AD3 as the independent reference
- The plant keys the SIL step definitions accept: noise, Coulomb friction, supply source resistance and current limit, encoder counts per revolution
- The `[METRIC]` lines the identification step prints, which Part E compares between the emulated and the real target

---

## Constraints & Limitations

- Nothing here runs until the board is powered from the stronger supply.
- A noise record is only valid for the sensor chain it was taken on.

## Open Questions

- Is the shaft accessible for the added-disk inertia test, and is the disk's mass known?
- Is a current probe available for the gain check, or only the AD3?
