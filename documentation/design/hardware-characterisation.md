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

| Quantity            | Value            | Provenance                                                                                                       |
|---------------------|------------------|------------------------------------------------------------------------------------------------------------------|
| Resistance          | 1.10 Ω           | Identified by the firmware, stable to 0.3 % over 38 runs                                                         |
| d-axis inductance   | 1.07 mH          | Identified; the same procedure reads 11 % low on the SIL bench plant, so the rig's true value may be near 1.2 mH |
| Pole pairs          | 4                | Identified                                                                                                       |
| Encoder             | 4000 counts      | Observed from the position steps                                                                                 |
| Inertia (with disk) | 12.4 µkg·m²      | Identified; reads about 10 % low on this encoder in SIL (Part D)                                                 |
| Flux linkage        | 0.007 Wb         | **Not measured**: derived from a claimed 0.042 Nm/A                                                              |
| Viscous friction    | 5 µNm·s/rad      | **Not measured**: a guess                                                                                        |
| Coulomb friction    | 0.5 mNm          | **Not measured**: a guess                                                                                        |
| Supply              | 24 V, 4 A → 10 A | The 4 A supply collapsed during runs; the 10 A one is pending                                                    |

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

The last row is the number the SIL needs most: it fixes the regressor noise that biases the
inertia (Part D). It is compared against the acceleration noise the plant predicts from its own
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

### Part D — Targets not met yet

These are defects found by the bench plant. Each joins `identification_robustness.feature` with
its fix, as the rule against held-out scenarios requires.

| Target                                                                               | Today                                                                                                       | Scenario to add                                               |
|--------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------|
| Inertia within 5 % on a quantised encoder                                            | 22 % low at 1000 counts, 10 % at 4000, 4 % at 20 000; both runs agree, so the agreement check cannot see it | A bench-plant calibration at 1000, 4000 and 20 000 counts     |
| A calibration of the light reference rotor completes under 0.2 mrad of encoder noise | Ends in Fault, inertia about 100× too small                                                                 | The teknic plant with `encoder_noise_sigma_radians` of 0.0002 |

The first is an errors-in-variables bias: the torque is regressed on the acceleration, whose
quantisation noise lies in the regressor. Regressing the acceleration on the torque and the speed
moves that noise into the output, where it does not bias the estimate. The estimator state and
its convergence metrics are then in the reciprocal inertia, which touches the estimator tests.

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

---

## Constraints & Limitations

- Nothing here runs until the board is powered from the stronger supply.
- A noise record is only valid for the sensor chain it was taken on.

## Open Questions

- Is the shaft accessible for the added-disk inertia test, and is the disk's mass known?
- Is a current probe available for the gain check, or only the AD3?
