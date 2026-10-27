Feature: Parameter Identification Against a Known Plant
  The firmware identifies the motor it drives: offline through the calibration
  procedures, online through the recursive estimators that run with the speed loop.
  Against a simulated plant whose parameters are known exactly, every estimate can
  be compared with the truth, on both reference motors. The limits are pinned from
  measured runs with a margin; the calibration record and the online estimates
  are read from the product's own trace and CAN response.

  The offline procedures and the online estimators hold their envelopes on both
  motors. The online estimators regress window averages the control interrupt
  accumulates over each outer-loop period, and the resistance is observable
  because the speed and position cascades carry a small d-axis square wave while
  the electrical estimator runs, in bursts of 4 s every 60 s from each enable. With a constant shaft torque the excitation
  waits on the plant's clock until the torque is on: the step that enables the
  motor waits in host time, so a slow host would otherwise start the rotor
  before the load arrives. See Part J of
  documentation/design/software-in-the-loop.md.

  Each requirement keeps one scenario here, on the bench motor, end to end
  through the emulated target. The sweeps over both reference motors, noise,
  seeds, speed and current laws, partial cogging, a shaft torque and a hot
  winding run in the host model-in-the-loop tests (integration_tests/
  model_in_the_loop), which drive the firmware's control-mode state machine and
  its identification services against the same plant model with the same
  limits.

  @sil @REQ-CAL-015
  Scenario: The full calibration measures the offsets through the bench's current-sense chain
    Given a bench motor plant
    And the plant reads its currents through the bench's current-sense chain
    And the motor boots in torque mode
    When the target boots
    And the full calibration is run
    Then the identified current offsets shall be within 5 mA of the plant
    And the identified resistance shall be within 5 % of the plant

  @sil @REQ-CAL-020
  Scenario: A calibration started while the rotor coasts measures the offsets once it is at rest
    Given a bench motor plant
    And a motor plant with:
      | adc_noise_sigma_ampere | 0.035  |
      | adc_bias_ampere_a      | 0.027  |
      | adc_bias_ampere_b      | -0.018 |
      | adc_bias_ampere_c      | 0.054  |
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 52 rad/s is applied
    Then the measured speed shall exceed 45 rad/s within 4000 ms
    When the motor is disabled
    And the full calibration is run
    Then the identified current offsets shall be within 5 mA of the plant
    And the identified resistance shall be within 5 % of the plant
    And the identified pole pairs shall match the plant

  @sil @REQ-CAL-014
  Scenario: The online estimators converge through the bench's current-sense chain
    Given a bench motor plant
    And the plant reads its currents through the bench's current-sense chain
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
      | inductance | 0.7 |
      | inertia    | 2.0 |
      | friction   | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online inertia estimate shall be within 2 % of the plant
    And the online friction estimate shall be within 15 % of the plant
    And the online resistance estimate shall be within 5 % of the plant
    And the online inductance estimate shall be within 10 % of the plant

  @sil @REQ-CAL-016
  Scenario: The online friction estimate is not misled by a dry friction that turns with the rotor
    Given a bench motor plant
    And a motor plant with:
      | coulomb_friction_nm | 0.0037 |
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 30 and 60 rad/s every 1000 ms for 4000 ms
    And the speed setpoint alternates between -30 and -60 rad/s every 1000 ms for 4000 ms
    And the online estimates are read
    Then the online friction estimate shall be within 10 % of the plant
    And the online inertia estimate shall be within 2 % of the plant

  @sil @REQ-CAL-017
  Scenario Outline: The online electrical estimates hold under the <current> current law when each duty acts a period late
    Given a bench motor plant
    And the plant applies each duty a period late, as the board does
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
    And the motor boots in speed mode
    And the current loop runs the <current> algorithm
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall be within 5 % of the plant
    And the online inductance estimate shall be within 10 % of the plant

    Examples:
      | current   |
      | pid       |

  @sil @REQ-CAL-019
  Scenario: The d-axis test current runs in a burst after enable and then pauses
    Given a bench motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 30 and 50 rad/s every 1000 ms for 9000 ms
    And the response is captured for 9000 ms after enable
    Then the d-axis current shall be above 60 mA RMS from 200 to 3800 ms after enable
    And the d-axis current shall be below 10 mA RMS from 4200 to 9000 ms after enable

  @sil @REQ-CAL-018
  Scenario: The full calibration maps the bench's cogging torque
    Given a bench motor plant
    And the plant's shaft has the bench's dry friction and cogging
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified cogging shall be within 10 % of the plant
    And the identified inertia shall be within 5 % of the plant
